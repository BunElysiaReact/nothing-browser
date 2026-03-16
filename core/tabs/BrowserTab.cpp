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

BrowserTab::BrowserTab(QWidget *parent) : QWidget(parent) {
    setupUI();
    setupWebEngine();
}

void BrowserTab::setupUI() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    auto *toolbar = new QWidget(this);
    toolbar->setFixedHeight(38);
    toolbar->setStyleSheet("background:#1a1a1a; border-bottom:1px solid #2a2a2a;");
    auto *tl = new QHBoxLayout(toolbar);
    tl->setContentsMargins(8,4,8,4);
    tl->setSpacing(6);

    m_urlBar = new QLineEdit(toolbar);
    m_urlBar->setPlaceholderText("https://");
    m_urlBar->setStyleSheet(R"(
        QLineEdit {
            background:#111111; color:#dddddd;
            border:1px solid #333333; border-radius:3px;
            padding:3px 8px; font-family:monospace; font-size:12px;
        }
        QLineEdit:focus { border-color:#0078d7; }
    )");
    connect(m_urlBar, &QLineEdit::returnPressed, this, &BrowserTab::navigate);

    m_goBtn = new QPushButton("GO", toolbar);
    m_goBtn->setFixedWidth(36);
    m_goBtn->setStyleSheet(R"(
        QPushButton {
            background:#0078d7; color:white; border:none;
            border-radius:3px; font-family:monospace;
            font-size:11px; font-weight:bold;
        }
        QPushButton:hover { background:#005fa3; }
    )");
    connect(m_goBtn, &QPushButton::clicked, this, &BrowserTab::navigate);

    auto makeToggle = [&](const QString &label, bool checked) {
        auto *cb = new QCheckBox(label, toolbar);
        cb->setChecked(checked);
        cb->setStyleSheet(R"(
            QCheckBox { color:#888888; font-size:11px; font-family:monospace; }
            QCheckBox:checked { color:#00cc66; }
            QCheckBox::indicator { width:12px; height:12px; }
        )");
        return cb;
    };

    m_jsToggle    = makeToggle("JS",  true);
    m_cssToggle   = makeToggle("CSS", true);
    m_imgToggle   = makeToggle("IMG", true);
    m_statusLabel = new QLabel("●", toolbar);
    m_statusLabel->setStyleSheet("color:#444444; font-size:10px;");

    connect(m_jsToggle,  &QCheckBox::toggled, this, &BrowserTab::toggleJS);
    connect(m_cssToggle, &QCheckBox::toggled, this, &BrowserTab::toggleCSS);
    connect(m_imgToggle, &QCheckBox::toggled, this, &BrowserTab::toggleImages);

    tl->addWidget(m_urlBar, 1);
    tl->addWidget(m_goBtn);
    tl->addSpacing(8);
    tl->addWidget(m_jsToggle);
    tl->addWidget(m_cssToggle);
    tl->addWidget(m_imgToggle);
    tl->addWidget(m_statusLabel);

    m_view = new QWebEngineView(this);
    root->addWidget(toolbar);
    root->addWidget(m_view, 1);
}

void BrowserTab::setupWebEngine() {
    auto &spoofer = FingerprintSpoofer::instance();

    auto *profile = new QWebEngineProfile(this);
    profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    profile->setHttpUserAgent(spoofer.identity().userAgent);

    // Spoof script — now runs on subframes too
    // Fixes: hidden iframe leaks that CreepJS uses to catch spoofing
    QWebEngineScript spoofScript;
    spoofScript.setName("nothing_fingerprint");
    QString _s = spoofer.injectionScript(); qDebug() << "[NB C++] script length:" << _s.size(); spoofScript.setSourceCode(_s);
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

    auto *page = new QWebEnginePage(profile, this);

    // Fixes hasKnownBgColor — QtWebEngine's default bg is detectable
    page->setBackgroundColor(Qt::white);

    m_view->setPage(page);

    m_capture = new NetworkCapture(this);
    m_capture->attachToPage(page, profile);

    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (!ok) return;
        m_view->page()->runJavaScript(
            "window.addEventListener('error', e => console.log('FP ERROR:', e.message, e.lineno));"
        );
        m_view->page()->runJavaScript(NetworkCapture::captureScript());
    });

    connect(m_view, &QWebEngineView::urlChanged, this, &BrowserTab::onUrlChanged);
    applySettings();
}

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

void BrowserTab::navigate() {
    QString raw = m_urlBar->text().trimmed();
    if (raw.isEmpty()) return;
    if (!raw.startsWith("http://") && !raw.startsWith("https://"))
        raw.prepend("https://");
    m_view->load(QUrl(raw));
    m_statusLabel->setStyleSheet("color:#0078d7; font-size:10px;");
}

void BrowserTab::onUrlChanged(const QUrl &url) {
    m_urlBar->setText(url.toString());
    m_statusLabel->setStyleSheet("color:#00cc66; font-size:10px;");
}

void BrowserTab::toggleJS(bool enabled) {
    m_view->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, enabled);
    m_view->reload();
}

void BrowserTab::toggleCSS(bool enabled) {
    if (!enabled) {
        m_view->page()->runJavaScript(R"(
            var s = document.createElement('style');
            s.id = '__nothing_nocss';
            s.innerHTML = '* { all: revert !important; }';
            document.head.appendChild(s);
        )");
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