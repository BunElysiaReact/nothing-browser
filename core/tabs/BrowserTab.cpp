#include "BrowserTab.h"
#include "../engine/Interceptor.h"
#include "../engine/FingerprintSpoofer.h"
#include "../engine/NetworkCapture.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineNewWindowRequest>
#include <QWebEngineHistory>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>

// ============================================================================
// Custom page to intercept nothing:// URLs before they reach the OS
// ============================================================================
class NothingWebPage : public QWebEnginePage {
    Q_OBJECT
public:
    using QWebEnginePage::QWebEnginePage;

signals:
    void nothingAction(const QUrl &url);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override {
        if (url.scheme() == "nothing") {
            emit nothingAction(url);
            return false;   // reject navigation – Qt will not forward to OS
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
};

// ── Helper: toolbar button style ─────────────────────────────────────────────
QString BrowserTab::toolbarButtonStyle(const QString &extra) {
    return QString(R"(
        QPushButton {
            background: transparent;
            color: #888888;
            border: none;
            border-radius: 4px;
            font-family: monospace;
            font-size: 14px;
            padding: 2px 6px;
            %1
        }
        QPushButton:hover  { background: #1e1e1e; color: #00ff88; }
        QPushButton:pressed{ background: #0a0a0a; color: #00ff88; }
    )").arg(extra);
}

// ─────────────────────────────────────────────────────────────────────────────
BrowserTab::BrowserTab(QWidget *parent) : QWidget(parent) {
    // Shortcuts file location
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    m_shortcutsPath = dataDir + "/shortcuts.json";

    // Default background = bundled asset
    m_bgImagePath = "qrc:/icons/main alysia.jpeg";

    loadShortcuts();
    setupUI();
    setupWebEngine();
}

// ── UI ────────────────────────────────────────────────────────────────────────
void BrowserTab::setupUI() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto *toolbar = new QWidget(this);
    toolbar->setFixedHeight(40);
    toolbar->setStyleSheet(
        "background: rgba(10,10,10,0.97);"
        "border-bottom: 1px solid #1a1a1a;"
    );
    auto *tl = new QHBoxLayout(toolbar);
    tl->setContentsMargins(8,4,8,4);
    tl->setSpacing(4);

    // Nav buttons
    m_backBtn    = new QPushButton("\u2190", toolbar);
    m_refreshBtn = new QPushButton("\u21bb", toolbar);
    m_homeBtn    = new QPushButton("\u2302", toolbar);

    for (auto *btn : {m_backBtn, m_refreshBtn, m_homeBtn}) {
        btn->setFixedSize(30, 28);
        btn->setStyleSheet(toolbarButtonStyle());
        btn->setCursor(Qt::PointingHandCursor);
    }
    // Home glows green always
    m_homeBtn->setStyleSheet(toolbarButtonStyle("color: #00ff88;"));

    connect(m_backBtn,    &QPushButton::clicked, this, &BrowserTab::goBack);
    connect(m_refreshBtn, &QPushButton::clicked, this, &BrowserTab::refreshPage);
    connect(m_homeBtn,    &QPushButton::clicked, this, &BrowserTab::goHome);

    // URL bar
    m_urlBar = new QLineEdit(toolbar);
    m_urlBar->setPlaceholderText("https://  or  search...");
    m_urlBar->setStyleSheet(R"STYLE(
        QLineEdit {
            background: #0d0d0d;
            color: #cccccc;
            border: 1px solid #222222;
            border-radius: 4px;
            padding: 4px 10px;
            font-family: "Courier New", monospace;
            font-size: 12px;
            selection-background-color: #00ff8833;
        }
        QLineEdit:focus {
            border-color: #00ff88;
            color: #ffffff;
            background: #111111;
        }
    )STYLE");
    connect(m_urlBar, &QLineEdit::returnPressed, this, &BrowserTab::navigate);

    m_goBtn = new QPushButton("\u25b6", toolbar);
    m_goBtn->setFixedSize(30, 28);
    m_goBtn->setStyleSheet(toolbarButtonStyle("color: #00ff88;"));
    m_goBtn->setCursor(Qt::PointingHandCursor);
    connect(m_goBtn, &QPushButton::clicked, this, &BrowserTab::navigate);

    // Status dot
    m_statusLabel = new QLabel("\u25cf", toolbar);
    m_statusLabel->setStyleSheet("color: #333333; font-size: 9px; padding-right:2px;");

    // Toggles
    auto makeToggle = [&](const QString &label, bool checked) {
        auto *cb = new QCheckBox(label, toolbar);
        cb->setChecked(checked);
        cb->setStyleSheet(R"STYLE(
            QCheckBox { color: #555555; font-size: 10px; font-family: monospace; }
            QCheckBox:checked { color: #00ff88; }
            QCheckBox::indicator { width: 10px; height: 10px; }
        )STYLE");
        return cb;
    };
    m_jsToggle  = makeToggle("JS",  true);
    m_cssToggle = makeToggle("CSS", true);
    m_imgToggle = makeToggle("IMG", true);

    connect(m_jsToggle,  &QCheckBox::toggled, this, &BrowserTab::toggleJS);
    connect(m_cssToggle, &QCheckBox::toggled, this, &BrowserTab::toggleCSS);
    connect(m_imgToggle, &QCheckBox::toggled, this, &BrowserTab::toggleImages);

    tl->addWidget(m_backBtn);
    tl->addWidget(m_refreshBtn);
    tl->addWidget(m_homeBtn);
    tl->addSpacing(4);
    tl->addWidget(m_urlBar, 1);
    tl->addWidget(m_goBtn);
    tl->addSpacing(6);
    tl->addWidget(m_jsToggle);
    tl->addWidget(m_cssToggle);
    tl->addWidget(m_imgToggle);
    tl->addWidget(m_statusLabel);

    m_view = new QWebEngineView(this);
    root->addWidget(toolbar);
    root->addWidget(m_view, 1);
}

// ── WebEngine setup ───────────────────────────────────────────────────────────
void BrowserTab::setupWebEngine() {
    auto &spoofer = FingerprintSpoofer::instance();

    auto *profile = new QWebEngineProfile(this);
    profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    profile->setHttpUserAgent(spoofer.identity().userAgent);

    QWebEngineScript spoofScript;
    spoofScript.setName("nothing_fingerprint");
    QString _s = spoofer.injectionScript();
    spoofScript.setSourceCode(_s);
    spoofScript.setInjectionPoint(QWebEngineScript::DocumentReady);
    spoofScript.setWorldId(QWebEngineScript::MainWorld);
    spoofScript.setRunsOnSubFrames(true);
    profile->scripts()->insert(spoofScript);

    QWebEngineScript capScript;
    capScript.setName("nothing_capture");
    capScript.setSourceCode(NetworkCapture::captureScript());
    capScript.setInjectionPoint(QWebEngineScript::DocumentReady);
    capScript.setWorldId(QWebEngineScript::MainWorld);
    capScript.setRunsOnSubFrames(true);
    profile->scripts()->insert(capScript);

    m_interceptor = new Interceptor(this);
    profile->setUrlRequestInterceptor(m_interceptor);
    connect(m_interceptor, &Interceptor::requestSeen,
            this,           &BrowserTab::requestCaptured);

    // Use our custom page that intercepts nothing:// URLs
    auto *page = new NothingWebPage(profile, this);
    page->setBackgroundColor(Qt::black);

    // Connect the custom signal to handle all nothing:// actions
    connect(page, &NothingWebPage::nothingAction, this, [this](const QUrl &url) {
        QString s = url.toString();

        if (s.startsWith("nothing://navigate?url=")) {
            QString dest = QUrl::fromPercentEncoding(
                s.mid(QString("nothing://navigate?url=").length()).toUtf8());
            m_onHomePage = false;
            m_view->load(QUrl(dest));
        }
        else if (s.startsWith("nothing://add-shortcut?")) {
            QUrlQuery q(url);
            QString name = q.queryItemValue("name", QUrl::FullyDecoded);
            QString url2 = q.queryItemValue("url",  QUrl::FullyDecoded);
            if (!name.isEmpty() && !url2.isEmpty()) {
                QJsonObject obj;
                obj["name"] = name;
                obj["url"]  = url2;
                m_shortcuts.append(obj);
                saveShortcuts();
                showHomePage();
            }
        }
        else if (s.startsWith("nothing://remove-shortcut?")) {
            QUrlQuery q(url);
            int idx = q.queryItemValue("index").toInt();
            if (idx >= 0 && idx < m_shortcuts.size()) {
                m_shortcuts.removeAt(idx);
                saveShortcuts();
                showHomePage();
            }
        }
        else if (s.startsWith("nothing://set-bg?path=")) {
            QString path = QUrl::fromPercentEncoding(
                s.mid(QString("nothing://set-bg?path=").length()).toUtf8());
            m_bgImagePath = path;
            showHomePage();
        }
    });

    // Handle new windows
    connect(page, &QWebEnginePage::newWindowRequested,
            this, [this](QWebEngineNewWindowRequest &req) {
        QUrl url = req.requestedUrl();
        if (url.isEmpty() || !url.isValid()) return;
        m_urlBar->setText(url.toString());
        m_view->load(url);
        req.openIn(m_view->page());
    });

    // Simple loadFinished handling (no urlChanged needed)
    connect(page, &QWebEnginePage::loadFinished, this, [this](bool ok) {
        if (!ok) return;
        m_view->page()->runJavaScript(
            "window.addEventListener('error', e => console.log('FP ERROR:', e.message));"
        );
        m_view->page()->runJavaScript(NetworkCapture::captureScript());
    });

    m_view->setPage(page);

    m_capture = new NetworkCapture(this);
    m_capture->attachToPage(page, profile);

    applySettings();
    PluginManager::instance().injectAll(profile);

    // Show home on startup
    showHomePage();
}

// ── Home page ─────────────────────────────────────────────────────────────────
void BrowserTab::showHomePage() {
    m_onHomePage = true;
    m_urlBar->setText("");
    m_urlBar->setPlaceholderText("https://  or  search...");
    m_statusLabel->setStyleSheet("color: #00ff88; font-size: 9px;");

    QString html = buildHomeHTML();
    m_view->setHtml(html, QUrl("nothing://home"));
}

QString BrowserTab::shortcutsJson() {
    QJsonDocument doc(m_shortcuts);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

QString BrowserTab::buildHomeHTML() {
    // Resolve background: qrc or filesystem
    QString bgCSS;
    if (m_bgImagePath.startsWith("qrc:/")) {
        bgCSS = QString("background-image: url('%1');").arg(m_bgImagePath);
    } else {
        bgCSS = QString("background-image: url('file://%1');").arg(m_bgImagePath);
    }

    QString shortcutsJS = shortcutsJson();

    // Provide fallback if shortcutsJS is empty or invalid
    QString safeShortcutsJS = shortcutsJS.isEmpty() ? "[]" : shortcutsJS;

    QString html =
        R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Rajdhani:wght@300;600&display=swap');

  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  html, body {
    width: 100%; height: 100%;
    background: #000;
    color: #ccc;
    font-family: 'Share Tech Mono', monospace;
    overflow: hidden;
  }

  /* ── Background ── */
  #bg {
    position: fixed; inset: 0;
    )HTML" + bgCSS + R"HTML(
    background-size: cover;
    background-position: center top;
    background-repeat: no-repeat;
    filter: brightness(0.38) saturate(0.8);
    z-index: 0;
  }
  /* scanline overlay */
  #bg::after {
    content: '';
    position: absolute; inset: 0;
    background: repeating-linear-gradient(
      0deg,
      transparent,
      transparent 2px,
      rgba(0,0,0,0.18) 2px,
      rgba(0,0,0,0.18) 4px
    );
    pointer-events: none;
  }

  /* ── Main container ── */
  #main {
    position: relative; z-index: 1;
    display: flex; flex-direction: column;
    align-items: center; justify-content: center;
    height: 100vh;
    gap: 0;
  }

  /* ── Logo / tagline ── */
  #logo-wrap {
    text-align: center;
    margin-bottom: 32px;
    animation: fadeDown 0.7s cubic-bezier(.22,1,.36,1) both;
  }
  #logo-title {
    font-family: 'Rajdhani', sans-serif;
    font-weight: 600;
    font-size: 52px;
    letter-spacing: 12px;
    color: #ffffff;
    text-transform: uppercase;
    text-shadow: 0 0 30px rgba(0,255,136,0.35), 0 0 60px rgba(0,255,136,0.12);
  }
  #logo-title span { color: #00ff88; }
  #logo-tag {
    font-size: 11px;
    letter-spacing: 5px;
    color: #00ff8866;
    margin-top: 6px;
    text-transform: uppercase;
  }

  /* ── Search bar ── */
  #search-wrap {
    position: relative;
    width: min(600px, 88vw);
    animation: fadeUp 0.7s 0.15s cubic-bezier(.22,1,.36,1) both;
  }
  #search-input {
    width: 100%;
    padding: 14px 50px 14px 20px;
    background: rgba(0,0,0,0.72);
    border: 1px solid #2a2a2a;
    border-radius: 6px;
    color: #e0e0e0;
    font-family: 'Share Tech Mono', monospace;
    font-size: 15px;
    outline: none;
    backdrop-filter: blur(12px);
    transition: border-color 0.25s, box-shadow 0.25s, background 0.25s;
    caret-color: #00ff88;
  }
  #search-input::placeholder { color: #444; }
  #search-input:focus {
    border-color: #00ff88;
    background: rgba(0,10,5,0.85);
    box-shadow: 0 0 0 1px #00ff8833, 0 0 24px rgba(0,255,136,0.15);
  }
  /* animated right border pulse when focused */
  #search-wrap::after {
    content: '';
    position: absolute;
    right: 14px; top: 50%; transform: translateY(-50%);
    width: 8px; height: 8px;
    border-radius: 50%;
    background: #00ff88;
    opacity: 0;
    transition: opacity 0.2s;
    pointer-events: none;
    box-shadow: 0 0 8px #00ff88;
  }
  #search-input:focus ~ #search-dot { opacity: 1; }
  #search-dot {
    position: absolute;
    right: 14px; top: 50%; transform: translateY(-50%);
    width: 8px; height: 8px;
    border-radius: 50%;
    background: #00ff88;
    opacity: 0;
    pointer-events: none;
    box-shadow: 0 0 8px #00ff88;
    transition: opacity 0.2s;
    animation: pulse 1.4s infinite;
  }
  #search-input:not(:placeholder-shown) ~ #search-dot { opacity: 1; }

  @keyframes pulse {
    0%,100% { box-shadow: 0 0 4px #00ff88; }
    50%      { box-shadow: 0 0 14px #00ff88, 0 0 28px #00ff8855; }
  }

  /* ── Shortcuts ── */
  #shortcuts-section {
    margin-top: 36px;
    width: min(700px, 92vw);
    animation: fadeUp 0.7s 0.28s cubic-bezier(.22,1,.36,1) both;
  }
  #shortcuts-grid {
    display: flex;
    flex-wrap: wrap;
    gap: 12px;
    justify-content: center;
  }
  .shortcut {
    display: flex; flex-direction: column;
    align-items: center; gap: 6px;
    cursor: pointer;
    width: 72px;
  }
  .shortcut-icon {
    width: 52px; height: 52px;
    border-radius: 12px;
    background: rgba(255,255,255,0.05);
    border: 1px solid #1e1e1e;
    display: flex; align-items: center; justify-content: center;
    font-size: 20px;
    transition: background 0.18s, border-color 0.18s, transform 0.18s;
    position: relative;
  }
  .shortcut:hover .shortcut-icon {
    background: rgba(0,255,136,0.07);
    border-color: #00ff8844;
    transform: translateY(-3px);
  }
  .shortcut-label {
    font-size: 10px;
    color: #888;
    text-align: center;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    width: 100%;
    transition: color 0.18s;
  }
  .shortcut:hover .shortcut-label { color: #ccc; }
  /* remove X */
  .shortcut-rm {
    position: absolute; top: -5px; right: -5px;
    width: 16px; height: 16px;
    border-radius: 50%;
    background: #1a0000;
    border: 1px solid #440000;
    color: #ff4444;
    font-size: 9px;
    display: none;
    align-items: center; justify-content: center;
    cursor: pointer;
    z-index: 2;
  }
  .shortcut:hover .shortcut-rm { display: flex; }

  /* Add shortcut button */
  #add-btn {
    width: 52px; height: 52px;
    border-radius: 12px;
    background: transparent;
    border: 1px dashed #2a2a2a;
    color: #444;
    font-size: 22px;
    cursor: pointer;
    transition: border-color 0.18s, color 0.18s;
    display: flex; align-items: center; justify-content: center;
  }
  #add-btn:hover { border-color: #00ff8855; color: #00ff88; }

  /* Add form */
  #add-form {
    display: none;
    margin-top: 14px;
    background: rgba(0,0,0,0.75);
    border: 1px solid #222;
    border-radius: 8px;
    padding: 14px 16px;
    gap: 8px;
    flex-direction: column;
    backdrop-filter: blur(10px);
  }
  #add-form.open { display: flex; }
  #add-form input {
    background: #0d0d0d;
    border: 1px solid #2a2a2a;
    border-radius: 4px;
    color: #ccc;
    font-family: 'Share Tech Mono', monospace;
    font-size: 12px;
    padding: 7px 10px;
    outline: none;
    caret-color: #00ff88;
  }
  #add-form input:focus { border-color: #00ff8855; }
  #add-form-row { display: flex; gap: 8px; }
  #add-form button {
    flex: 1;
    padding: 7px;
    border: none; border-radius: 4px;
    font-family: 'Share Tech Mono', monospace;
    font-size: 11px;
    cursor: pointer;
  }
  #btn-save { background: #00ff8822; color: #00ff88; border: 1px solid #00ff8833; }
  #btn-save:hover { background: #00ff8833; }
  #btn-cancel { background: #1a1a1a; color: #666; }
  #btn-cancel:hover { color: #aaa; }

  /* ── Set BG hint ── */
  #bg-hint {
    position: fixed; bottom: 14px; right: 16px;
    font-size: 10px; color: #2a2a2a;
    cursor: pointer;
    transition: color 0.2s;
    z-index: 10;
  }
  #bg-hint:hover { color: #00ff8888; }

  /* ── Animations ── */
  @keyframes fadeDown {
    from { opacity:0; transform: translateY(-18px); }
    to   { opacity:1; transform: translateY(0); }
  }
  @keyframes fadeUp {
    from { opacity:0; transform: translateY(16px); }
    to   { opacity:1; transform: translateY(0); }
  }
</style>
</head>
<body>

<div id="bg"></div>

<div id="main">
  <!-- Logo -->
  <div id="logo-wrap">
    <div id="logo-title">NOTH<span>I</span>NG</div>
    <div id="logo-tag">Does Nothing &nbsp;&middot;&nbsp; except  &nbsp;&middot;&nbsp; everything</div>
  </div>

  <!-- Search -->
  <div id="search-wrap">
    <input id="search-input" type="text" placeholder="search or enter address..." autocomplete="off" spellcheck="false" />
    <div id="search-dot"></div>
  </div>

  <!-- Shortcuts -->
  <div id="shortcuts-section">
    <div id="shortcuts-grid"></div>
    <div id="add-form">
      <input id="inp-name" type="text" placeholder="Name (e.g. GitHub)" />
      <input id="inp-url"  type="text" placeholder="URL (e.g. https://github.com)" />
      <div id="add-form-row">
        <button id="btn-save">+ ADD</button>
        <button id="btn-cancel">CANCEL</button>
      </div>
    </div>
  </div>
</div>

<!-- BG change hint -->
<div id="bg-hint" title="Change background image">&#x2B21; bg</div>

<script>
// ── Data ──────────────────────────────────────────────────────────────────────
var shortcutsRaw = )HTML" + safeShortcutsJS + R"HTML(;
// Ensure shortcuts is an array (fallback if JSON is invalid or empty)
var shortcuts = Array.isArray(shortcutsRaw) ? shortcutsRaw : [];

// ── Favicon helper (safe) ────────────────────────────────────────────────────
function faviconFor(url) {
  try {
    var host = new URL(url).hostname;
    return 'https://www.google.com/s2/favicons?sz=64&domain=' + host;
  } catch(e) {
    return null;
  }
}

function initial(name) {
  return (name || '?')[0].toUpperCase();
}

// ── Render shortcuts ──────────────────────────────────────────────────────────
function renderShortcuts() {
  var grid = document.getElementById('shortcuts-grid');
  if (!grid) return;
  grid.innerHTML = '';

  shortcuts.forEach(function(sc, i) {
    var wrap = document.createElement('div');
    wrap.className = 'shortcut';

    var icon = document.createElement('div');
    icon.className = 'shortcut-icon';

    var fav = faviconFor(sc.url);
    if (fav) {
      var img = document.createElement('img');
      img.src = fav;
      img.width = 28; img.height = 28;
      img.style.borderRadius = '6px';
      img.onerror = function() { img.style.display='none'; icon.textContent = initial(sc.name); };
      icon.appendChild(img);
    } else {
      icon.textContent = initial(sc.name);
    }

    // Remove button
    var rm = document.createElement('div');
    rm.className = 'shortcut-rm';
    rm.textContent = '\u2715';
    rm.addEventListener('click', function(e) {
      e.stopPropagation();
      window.location = 'nothing://remove-shortcut?index=' + i;
    });
    icon.appendChild(rm);

    var label = document.createElement('div');
    label.className = 'shortcut-label';
    label.textContent = sc.name;

    wrap.appendChild(icon);
    wrap.appendChild(label);
    wrap.addEventListener('click', function() {
      navigateTo(sc.url);
    });

    grid.appendChild(wrap);
  });

  // Add button
  var addWrap = document.createElement('div');
  addWrap.className = 'shortcut';
  var addBtn = document.createElement('div');
  addBtn.id = 'add-btn';
  addBtn.textContent = '+';
  addBtn.addEventListener('click', function() {
    var form = document.getElementById('add-form');
    if (form) form.classList.toggle('open');
  });
  addWrap.appendChild(addBtn);
  grid.appendChild(addWrap);
}

// ── Navigation ────────────────────────────────────────────────────────────────
function navigateTo(raw) {
  var url = raw.trim();
  if (!url) return;
  if (!/^https?:\/\//i.test(url)) {
    if (/^[\w\-]+\.[\w]{2,}/.test(url)) {
      url = 'https://' + url;
    } else {
      url = 'https://www.google.com/search?q=' + encodeURIComponent(url);
    }
  }
  window.location = 'nothing://navigate?url=' + encodeURIComponent(url);
}

// ── Search bar events ─────────────────────────────────────────────────────────
document.getElementById('search-input').addEventListener('keydown', function(e) {
  if (e.key === 'Enter') {
    navigateTo(this.value);
  }
});

// ── Add shortcut ──────────────────────────────────────────────────────────────
document.getElementById('btn-save').addEventListener('click', function() {
  var name = document.getElementById('inp-name').value.trim();
  var url  = document.getElementById('inp-url').value.trim();
  if (!name || !url) return;
  if (!/^https?:\/\//i.test(url)) url = 'https://' + url;
  window.location = 'nothing://add-shortcut?name=' + encodeURIComponent(name)
                  + '&url=' + encodeURIComponent(url);
});

document.getElementById('btn-cancel').addEventListener('click', function() {
  var form = document.getElementById('add-form');
  if (form) form.classList.remove('open');
});

// ── BG change ─────────────────────────────────────────────────────────────────
document.getElementById('bg-hint').addEventListener('click', function() {
  var path = prompt('Enter full path to image file:\n(leave blank to reset to default)');
  if (path === null) return;
  if (path.trim() === '') {
    window.location = 'nothing://set-bg?path=' + encodeURIComponent('qrc:/icons/main alysia.jpeg');
  } else {
    window.location = 'nothing://set-bg?path=' + encodeURIComponent(path.trim());
  }
});

// ── Init ──────────────────────────────────────────────────────────────────────
renderShortcuts();
document.getElementById('search-input').focus();
</script>

</body>
</html>)HTML";

    return html;
}

// ── Shortcuts persistence ─────────────────────────────────────────────────────
void BrowserTab::loadShortcuts() {
    QFile f(m_shortcutsPath);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isArray()) m_shortcuts = doc.array();
}

void BrowserTab::saveShortcuts() {
    QFile f(m_shortcutsPath);
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(m_shortcuts).toJson());
}

// ── Settings ──────────────────────────────────────────────────────────────────
void BrowserTab::applySettings() {
    auto *s = m_view->settings();
    s->setAttribute(QWebEngineSettings::JavascriptEnabled,           true);
    s->setAttribute(QWebEngineSettings::AutoLoadImages,              true);
    s->setAttribute(QWebEngineSettings::PluginsEnabled,              true);
    s->setAttribute(QWebEngineSettings::WebGLEnabled,                true);
    s->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled,  true);
    s->setAttribute(QWebEngineSettings::FullScreenSupportEnabled,    false);
    s->setAttribute(QWebEngineSettings::AutoLoadIconsForPage,        false);
    s->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
}

// ── Navigation ────────────────────────────────────────────────────────────────
void BrowserTab::navigate() {
    QString raw = m_urlBar->text().trimmed();
    if (raw.isEmpty()) return;
    if (!raw.startsWith("http://") && !raw.startsWith("https://"))
        raw.prepend("https://");
    m_onHomePage = false;
    m_view->load(QUrl(raw));
    m_statusLabel->setStyleSheet("color: #0078d7; font-size: 9px;");
}

void BrowserTab::navigateToUrl(const QUrl &url) {
    m_onHomePage = false;
    m_urlBar->setText(url.toString());
    m_view->load(url);
}

void BrowserTab::onUrlChanged(const QUrl &url) {
    // Only called from outside (e.g., user types in bar, or after load)
    QString s = url.toString();
    if (s.startsWith("nothing://")) return;
    m_urlBar->setText(s);
    m_statusLabel->setStyleSheet("color: #00ff88; font-size: 9px;");
}

void BrowserTab::goHome() {
    showHomePage();
}

void BrowserTab::goBack() {
    if (m_view->history()->canGoBack())
        m_view->back();
    else
        showHomePage();
}

void BrowserTab::refreshPage() {
    if (m_onHomePage)
        showHomePage();
    else
        m_view->reload();
}

// ── Toggles ───────────────────────────────────────────────────────────────────
void BrowserTab::toggleJS(bool enabled) {
    m_view->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, enabled);
    m_view->reload();
}

void BrowserTab::toggleCSS(bool enabled) {
    if (!enabled) {
        m_view->page()->runJavaScript(R"JS(
            var s = document.createElement('style');
            s.id = '__nothing_nocss';
            s.innerHTML = '* { all: revert !important; }';
            document.head.appendChild(s);
        )JS");
    } else {
        m_view->page()->runJavaScript(
            "var el=document.getElementById('__nothing_nocss');"
            "if(el) el.remove();");
    }
}

void BrowserTab::toggleImages(bool enabled) {
    m_view->settings()->setAttribute(QWebEngineSettings::AutoLoadImages, enabled);
    m_view->reload();
}

void BrowserTab::runJS(const QString &script) {
    if (m_view && m_view->page())
        m_view->page()->runJavaScript(script);
}

// Required for the Q_OBJECT macro inside NothingWebPage
#include "BrowserTab.moc"