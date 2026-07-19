#include "PiggyTab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QHeaderView>
#include <QMessageBox>

// ─────────────────────────────────────────────────────────────────────────────
//  JS: full DOM tree extractor
// ─────────────────────────────────────────────────────────────────────────────
QString PiggyTab::domExtractorJS() {
    return R"JS(
(function extractDOM(el, depth) {
    if (depth > 60) return null;
    if (!el || el.nodeType !== 1) return null;

    var tag  = el.tagName.toLowerCase();
    var id   = el.id || '';
    var cls  = el.className && typeof el.className === 'string'
                   ? el.className.trim() : '';
    // Direct text nodes first; fall back to full innerText so containers
    // like <article> or <section> show their nested content instead of blank.
    var text = '';
    for (var i = 0; i < el.childNodes.length; i++) {
        var n = el.childNodes[i];
        if (n.nodeType === 3) text += n.nodeValue.trim();
    }
    if (!text && el.innerText) {
        // Collapse whitespace/newlines and cap at 200 chars
        text = el.innerText.replace(/\s+/g, ' ').trim();
    }
    text = text.slice(0, 200);

    var attrs = {};
    for (var a = 0; a < el.attributes.length; a++)
        attrs[el.attributes[a].name] = el.attributes[a].value.slice(0, 300);

    var selector = tag;
    if (id)       selector += '#' + id;
    else if (cls) selector += '.' + cls.split(' ')[0];

    var children = [];
    for (var c = 0; c < el.children.length; c++) {
        var child = extractDOM(el.children[c], depth + 1);
        if (child) children.push(child);
    }

    return { tag, id, cls, text, attrs, selector, children };
})(document.documentElement, 0);
)JS";
}

// ─────────────────────────────────────────────────────────────────────────────
//  JS: highlight matching elements on the live page
// ─────────────────────────────────────────────────────────────────────────────
QString PiggyTab::highlightJS(const QString &selector) {
    QString safe = selector;
    safe.replace("'", "\\'");
    return QString(R"JS(
(function() {
    document.querySelectorAll('.__piggy_hl__').forEach(function(el) {
        el.style.outline    = el.__piggy_old_outline__ || '';
        el.style.boxShadow  = el.__piggy_old_shadow__  || '';
        el.classList.remove('.__piggy_hl__');
    });
    try {
        document.querySelectorAll('%1').forEach(function(el) {
            el.__piggy_old_outline__ = el.style.outline;
            el.__piggy_old_shadow__  = el.style.boxShadow;
            el.style.outline   = '2px solid #00cc66';
            el.style.boxShadow = 'inset 0 0 0 2px rgba(0,204,102,0.15)';
            el.classList.add('__piggy_hl__');
            el.scrollIntoView({ behavior: 'smooth', block: 'center' });
        });
    } catch(e) {}
})();
)JS").arg(safe);
}

QString PiggyTab::clearHighlightJS() {
    return R"JS(
(function() {
    document.querySelectorAll('.__piggy_hl__').forEach(function(el) {
        el.style.outline   = el.__piggy_old_outline__ || '';
        el.style.boxShadow = el.__piggy_old_shadow__  || '';
        el.classList.remove('__piggy_hl__');
    });
})();
)JS";
}

// ─────────────────────────────────────────────────────────────────────────────
//  JS: click listener — fired when user clicks an element on the live page.
//  Posts a JSON message via window.__piggy_click__ which Qt polls.
//  We store it on window so Qt can grab it with runJavaScript.
// ─────────────────────────────────────────────────────────────────────────────
QString PiggyTab::clickListenerJS() {
    return R"JS(
(function() {
    if (window.__piggy_click_installed__) return;
    window.__piggy_click_installed__ = true;
    window.__piggy_pending_click__   = null;

    // Use mousedown + cancel click so we intercept BEFORE the browser
    // follows any <a> href or form submit. 'click' fires too late.
    function handleInspectClick(e) {
        if (!window.__piggy_inspect_active__) return;
        e.preventDefault();
        e.stopImmediatePropagation();

        var el  = e.target;
        var tag = el.tagName.toLowerCase();
        var id  = el.id || '';
        var cls = (el.className && typeof el.className === 'string')
                    ? el.className.trim() : '';

        var sel = tag;
        if (id)       sel += '#' + id;
        else if (cls) sel += '.' + cls.split(' ')[0];

        var rect = el.getBoundingClientRect();

        window.__piggy_pending_click__ = JSON.stringify({
            selector: sel,
            tag:      tag,
            id:       id,
            cls:      cls,
            w:        Math.round(rect.width),
            h:        Math.round(rect.height)
        });
    }

    // mousedown fires before navigation; also swallow click so links don't fire
    document.addEventListener('mousedown', handleInspectClick, true);
    document.addEventListener('click',     function(e) {
        if (window.__piggy_inspect_active__) {
            e.preventDefault();
            e.stopImmediatePropagation();
        }
    }, true);
})();
)JS";
}

// ─────────────────────────────────────────────────────────────────────────────
//  JS: hover tooltip — shows a little label above the hovered element
// ─────────────────────────────────────────────────────────────────────────────
QString PiggyTab::hoverTooltipJS() {
    return R"JS(
(function() {
    if (window.__piggy_tooltip_installed__) return;
    window.__piggy_tooltip_installed__ = true;

    var tip = document.createElement('div');
    tip.id = '__piggy_tooltip__';
    tip.style.cssText = [
        'position:fixed',
        'z-index:2147483647',
        'pointer-events:none',
        'background:rgba(0,0,0,0.82)',
        'color:#00cc66',
        'font:bold 11px/1.4 monospace',
        'padding:3px 8px',
        'border-radius:3px',
        'white-space:nowrap',
        'display:none',
        'border:1px solid #00cc66'
    ].join(';');
    document.body.appendChild(tip);

    function getSelector(el) {
        var tag = el.tagName.toLowerCase();
        var id  = el.id || '';
        var cls = (el.className && typeof el.className === 'string')
                    ? el.className.trim().split(' ')[0] : '';
        var s = tag;
        if (id)       s += '#' + id;
        else if (cls) s += '.' + cls;
        return s;
    }

    document.addEventListener('mouseover', function(e) {
        if (!window.__piggy_inspect_active__) { tip.style.display = 'none'; return; }
        var el   = e.target;
        if (el === tip) return;
        var rect = el.getBoundingClientRect();
        var sel  = getSelector(el);

        // highlight outline
        if (window.__piggy_hover_last__ && window.__piggy_hover_last__ !== el) {
            window.__piggy_hover_last__.style.outline = window.__piggy_hover_last__.__piggy_hov_outline__ || '';
        }
        el.__piggy_hov_outline__  = el.style.outline;
        el.style.outline          = '2px dashed #ff6688';
        window.__piggy_hover_last__ = el;

        tip.textContent = sel + '  ' + Math.round(rect.width) + '×' + Math.round(rect.height);
        tip.style.display = 'block';
        tip.style.left    = Math.min(e.clientX + 12, window.innerWidth - tip.offsetWidth - 8) + 'px';
        tip.style.top     = Math.max(e.clientY - 28, 4) + 'px';
    }, true);

    document.addEventListener('mousemove', function(e) {
        if (!window.__piggy_inspect_active__) return;
        tip.style.left = Math.min(e.clientX + 12, window.innerWidth - tip.offsetWidth - 8) + 'px';
        tip.style.top  = Math.max(e.clientY - 28, 4) + 'px';
    }, true);

    document.addEventListener('mouseout', function() {
        if (!window.__piggy_inspect_active__) return;
        tip.style.display = 'none';
        if (window.__piggy_hover_last__) {
            window.__piggy_hover_last__.style.outline = window.__piggy_hover_last__.__piggy_hov_outline__ || '';
            window.__piggy_hover_last__ = null;
        }
    }, true);
})();
)JS";
}

QString PiggyTab::removeHoverTooltipJS() {
    return R"JS(
(function() {
    window.__piggy_inspect_active__ = false;
    var t = document.getElementById('__piggy_tooltip__');
    if (t) t.style.display = 'none';
    if (window.__piggy_hover_last__) {
        window.__piggy_hover_last__.style.outline =
            window.__piggy_hover_last__.__piggy_hov_outline__ || '';
        window.__piggy_hover_last__ = null;
    }
})();
)JS";
}

// ─────────────────────────────────────────────────────────────────────────────
PiggyTab::PiggyTab(QWidget *parent) : QWidget(parent) {
    setupUI();
    setupEngine();

    // Re-enforce splitter sizes after engine wires up m_mirror's page.
    // Qt doesn't always propagate initial sizes through the full parent chain
    // until everything is attached, so we set it again here to be sure.
    m_splitter->setSizes({ 320, 680 });

    m_domPollTimer = new QTimer(this);
    m_domPollTimer->setInterval(1500);
    connect(m_domPollTimer, &QTimer::timeout, this, &PiggyTab::pollDomChanges);

    // ── Click-polling timer — checks for pending click events from JS every 200ms
    auto *clickPoll = new QTimer(this);
    clickPoll->setInterval(200);
    connect(clickPoll, &QTimer::timeout, this, [this]() {
        if (!m_inspectModeActive || !m_page) return;
        m_page->runJavaScript(
            "(function(){ var v = window.__piggy_pending_click__; "
            "window.__piggy_pending_click__ = null; return v; })()",
            [this](const QVariant &result) {
                if (result.isNull() || !result.isValid()) return;
                QString json = result.toString();
                if (json.isEmpty()) return;

                QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
                if (doc.isNull()) return;
                QJsonObject obj = doc.object();

                QString sel = obj["selector"].toString();
                QString tag = obj["tag"].toString();
                QString id  = obj["id"].toString();
                QString cls = obj["cls"].toString();
                int w = obj["w"].toInt();
                int h = obj["h"].toInt();

                onPageElementClicked(sel, tag, id, cls, w, h);
            }
        );
    });
    clickPoll->start();
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::setupUI() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top toolbar ───────────────────────────────────────────────────────────
    auto *toolbar = new QWidget(this);
    toolbar->setFixedHeight(40);
    toolbar->setStyleSheet("background:#111111; border-bottom:1px solid #222222;");
    auto *tl = new QHBoxLayout(toolbar);
    tl->setContentsMargins(10, 5, 10, 5);
    tl->setSpacing(6);

    auto *pigLabel = new QLabel("🐷 PIGGY TAB", toolbar);
    pigLabel->setStyleSheet("color:#ff6688; font-family:monospace; font-size:11px;"
                            "font-weight:bold; letter-spacing:2px;");

    m_urlBar = new QLineEdit(toolbar);
    m_urlBar->setPlaceholderText("Paste any URL and press GO...");
    m_urlBar->setStyleSheet(R"(
        QLineEdit {
            background:#0d0d0d; color:#dddddd;
            border:1px solid #333333; border-radius:3px;
            padding:3px 10px; font-family:monospace; font-size:12px;
        }
        QLineEdit:focus { border-color:#ff6688; }
    )");
    connect(m_urlBar, &QLineEdit::returnPressed, this, &PiggyTab::onNavigate);

    m_goBtn = new QPushButton("GO", toolbar);
    m_goBtn->setFixedWidth(40);
    m_goBtn->setStyleSheet(R"(
        QPushButton {
            background:#ff6688; color:#000000; border:none;
            border-radius:3px; font-family:monospace;
            font-size:11px; font-weight:bold;
        }
        QPushButton:hover { background:#cc4466; color:#ffffff; }
    )");
    connect(m_goBtn, &QPushButton::clicked, this, &PiggyTab::onNavigate);

    // ── NEW: Inspect mode toggle button ───────────────────────────────────────
    m_inspectBtn = new QPushButton("👁 INSPECT", toolbar);
    m_inspectBtn->setCheckable(true);
    m_inspectBtn->setFixedWidth(82);
    m_inspectBtn->setStyleSheet(R"(
        QPushButton {
            background:#1a1a1a; color:#888888; border:1px solid #333333;
            border-radius:3px; font-family:monospace; font-size:11px;
        }
        QPushButton:hover   { background:#222222; color:#ff6688; border-color:#ff6688; }
        QPushButton:checked {
            background:#1a0a10; color:#ff6688;
            border:1px solid #ff6688;
        }
    )");
    connect(m_inspectBtn, &QPushButton::toggled, this, &PiggyTab::toggleInspectMode);

    m_statusLabel = new QLabel("●", toolbar);
    m_statusLabel->setStyleSheet("color:#333333; font-size:10px;");

    tl->addWidget(pigLabel);
    tl->addSpacing(8);
    tl->addWidget(m_urlBar, 1);
    tl->addWidget(m_goBtn);
    tl->addWidget(m_inspectBtn);   // NEW
    tl->addWidget(m_statusLabel);

    root->addWidget(toolbar);

    // ── Main splitter: [tree panel | live mirror] ─────────────────────────────
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(2);
    m_splitter->setStyleSheet("QSplitter::handle { background:#1e1e1e; }");

    // ── Left panel ────────────────────────────────────────────────────────────
    auto *leftPanel = new QWidget(m_splitter);
    leftPanel->setMinimumWidth(300);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    // Selector bar
    auto *selectorRow = new QWidget(leftPanel);
    selectorRow->setFixedHeight(34);
    selectorRow->setStyleSheet("background:#0d0d0d; border-bottom:1px solid #1e1e1e;");
    auto *sl = new QHBoxLayout(selectorRow);
    sl->setContentsMargins(8, 4, 8, 4);
    sl->setSpacing(6);

    auto *sLabel = new QLabel("⌖", selectorRow);
    sLabel->setStyleSheet("color:#ff6688; font-size:14px;");

    m_selectorBar = new QLineEdit(selectorRow);
    m_selectorBar->setPlaceholderText("CSS selector or XPath to test...");
    m_selectorBar->setStyleSheet(R"(
        QLineEdit {
            background:#111111; color:#dddddd;
            border:1px solid #2a2a2a; border-radius:2px;
            padding:2px 8px; font-family:monospace; font-size:11px;
        }
        QLineEdit:focus { border-color:#ff6688; }
    )");
    connect(m_selectorBar, &QLineEdit::textChanged,
            this, &PiggyTab::onSelectorChanged);

    sl->addWidget(sLabel);
    sl->addWidget(m_selectorBar, 1);
    leftLayout->addWidget(selectorRow);

    // DOM tree
    m_tree = new QTreeWidget(leftPanel);
    m_tree->setHeaderLabels({"Element", "ID / Class", "Text"});
    m_tree->header()->setStretchLastSection(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setStyleSheet(R"(
        QTreeWidget {
            background:#0a0a0a; color:#cccccc;
            border:none; font-family:monospace; font-size:11px;
            selection-background-color:#1a2a1a;
        }
        QTreeWidget::item:hover { background:#111111; }
        QTreeWidget::item:selected { background:#1a2a1a; color:#00cc66; }
        QHeaderView::section {
            background:#111111; color:#555555;
            border:none; border-right:1px solid #1e1e1e;
            padding:4px; font-family:monospace; font-size:10px;
        }
    )");
    connect(m_tree, &QTreeWidget::itemClicked,
            this, &PiggyTab::onTreeItemClicked);
    leftLayout->addWidget(m_tree, 1);

    // Export button
    m_exportBtn = new QPushButton("⬇  EXPORT SELECTED DIV", leftPanel);
    m_exportBtn->setFixedHeight(30);
    m_exportBtn->setStyleSheet(R"(
        QPushButton {
            background:#1a1a1a; color:#888888;
            border:none; border-top:1px solid #2a2a2a;
            font-family:monospace; font-size:11px;
        }
        QPushButton:hover { background:#222222; color:#00cc66; }
    )");
    connect(m_exportBtn, &QPushButton::clicked, this, &PiggyTab::onExportClicked);
    leftLayout->addWidget(m_exportBtn);

    // ── Right panel: live mirror ──────────────────────────────────────────────
    m_mirror = new QWebEngineView(m_splitter);
    m_mirror->setMinimumWidth(400);   // can NEVER be squished to zero

    m_splitter->addWidget(leftPanel);
    m_splitter->addWidget(m_mirror);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 2);
    // Hard pixel sizes — stretch factor alone is not guaranteed on first paint
    m_splitter->setSizes({ 320, 680 });

    // ── Bottom info panel ─────────────────────────────────────────────────────
    auto *bottomSplit = new QSplitter(Qt::Vertical, this);
    bottomSplit->addWidget(m_splitter);

    m_infoPanel = new QTextEdit(this);
    m_infoPanel->setReadOnly(true);
    m_infoPanel->setFixedHeight(130);
    m_infoPanel->setStyleSheet(R"(
        QTextEdit {
            background:#080808; color:#888888;
            border:none; border-top:1px solid #1e1e1e;
            font-family:monospace; font-size:11px;
            padding:6px;
        }
    )");
    m_infoPanel->setPlaceholderText(
        "Click any element in the tree — or enable 👁 INSPECT and click live page...");
    bottomSplit->addWidget(m_infoPanel);
    bottomSplit->setStretchFactor(0, 1);
    bottomSplit->setStretchFactor(1, 0);

    root->addWidget(bottomSplit, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::setupEngine() {
    m_profile = new QWebEngineProfile(this);
    m_profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    m_page = new QWebEnginePage(m_profile, this);
    m_page->setBackgroundColor(Qt::white);
    m_mirror->setPage(m_page);

    auto *s = m_mirror->settings();
    s->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    s->setAttribute(QWebEngineSettings::AutoLoadImages,    true);

    connect(m_mirror, &QWebEngineView::loadFinished,
            this, &PiggyTab::onPageLoaded);

    // ── Show a placeholder so the right panel is visibly alive from launch ────
    m_page->setHtml(R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  * { margin:0; padding:0; box-sizing:border-box; }
  body {
    background: #0a0a0a;
    display: flex; flex-direction: column;
    align-items: center; justify-content: center;
    height: 100vh;
    font-family: monospace;
    color: #333333;
    user-select: none;
  }
  .pig  { font-size: 56px; margin-bottom: 18px; opacity: 0.6; }
  .head { font-size: 13px; color: #ff6688; letter-spacing: 3px; margin-bottom: 10px; }
  .sub  { font-size: 11px; color: #2a2a2a; letter-spacing: 1px; }
  .arrow { display: inline-block; animation: pulse 2s infinite; }
  @keyframes pulse { 0%,100%{opacity:.3} 50%{opacity:1} }
</style>
</head>
<body>
  <div class="pig">🐷</div>
  <div class="head">PIGGY TAB</div>
  <div class="sub"><span class="arrow">&#x2191;</span>&nbsp; paste a URL and hit GO</div>
</body>
</html>
)HTML");
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::onNavigate() {
    QString raw = m_urlBar->text().trimmed();
    if (raw.isEmpty()) return;
    if (!raw.startsWith("http://") && !raw.startsWith("https://"))
        raw.prepend("https://");

    m_statusLabel->setStyleSheet("color:#0078d7; font-size:10px;");
    m_tree->clear();
    m_infoPanel->clear();
    m_inspectBtn->setChecked(false);
    m_inspectModeActive = false;
    m_mirror->load(QUrl(raw));
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::onPageLoaded(bool ok) {
    if (!ok) {
        m_statusLabel->setStyleSheet("color:#cc0000; font-size:10px;");
        return;
    }
    m_statusLabel->setStyleSheet("color:#00cc66; font-size:10px;");
    m_urlBar->setText(m_mirror->url().toString());
    injectDomExtractor();
    injectClickListener();   // NEW — always inject, activation controlled by flag
    m_domPollTimer->start();
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::injectClickListener() {
    m_page->runJavaScript(clickListenerJS());
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::toggleInspectMode(bool enabled) {
    m_inspectModeActive = enabled;

    if (enabled) {
        // Activate the JS flag and inject hover tooltip
        m_page->runJavaScript("window.__piggy_inspect_active__ = true;");
        injectHoverTooltip();
        m_infoPanel->setPlaceholderText("Inspect mode ON — hover to preview, click to select element...");
    } else {
        m_page->runJavaScript("window.__piggy_inspect_active__ = false;");
        removeHoverTooltip();
        m_infoPanel->setPlaceholderText("Click any element in the tree — or enable 👁 INSPECT and click live page...");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::injectHoverTooltip() {
    m_page->runJavaScript(hoverTooltipJS());
}

void PiggyTab::removeHoverTooltip() {
    m_page->runJavaScript(removeHoverTooltipJS());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Called when JS fires a click event back to us via the polling mechanism
// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::onPageElementClicked(const QString &selector, const QString &tag,
                                    const QString &id,  const QString &cls,
                                    int w, int h) {
    // 1. Highlight on page
    if (!selector.isEmpty())
        m_page->runJavaScript(highlightJS(selector));

    // 2. Update info panel
    updateInfoHUD(tag, cls, selector, w, h);

    // 3. Try to find and select matching tree item
    selectTreeItemBySelector(selector);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Walk the QTreeWidget to find the item whose selector matches, then select it
// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::selectTreeItemBySelector(const QString &selector) {
    // BFS through all items
    QList<QTreeWidgetItem*> queue;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        queue.append(m_tree->topLevelItem(i));

    while (!queue.isEmpty()) {
        auto *item = queue.takeFirst();

        QVariant data  = item->data(0, Qt::UserRole);
        QJsonDocument doc = QJsonDocument::fromVariant(data);
        if (!doc.isNull()) {
            QJsonObject obj = doc.object();
            if (obj["selector"].toString() == selector) {
                m_tree->setCurrentItem(item);
                m_tree->scrollToItem(item);
                // Also populate info panel from the tree data
                onTreeItemClicked(item, 0);
                return;
            }
        }

        for (int c = 0; c < item->childCount(); ++c)
            queue.append(item->child(c));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::updateInfoHUD(const QString &tag, const QString &cls,
                              const QString &sel, int w, int h) {
    QString info;
    info += QString("TAG:       %1\n").arg(tag);
    info += QString("CLASS:     %1\n").arg(cls);
    info += QString("SELECTOR:  %1\n").arg(sel);
    info += QString("SIZE:      %1 × %2 px\n").arg(w).arg(h);
    m_infoPanel->setPlainText(info);
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::injectDomExtractor() {
    m_page->runJavaScript(domExtractorJS(), [this](const QVariant &result) {
        if (result.isNull() || !result.isValid()) return;

        QJsonDocument doc = QJsonDocument::fromVariant(result);
        if (doc.isNull()) return;

        QJsonObject root = doc.object();
        m_tree->clear();

        auto *rootItem = new QTreeWidgetItem(m_tree);
        rootItem->setText(0, root["tag"].toString());
        rootItem->setText(1, (root["id"].toString().isEmpty() ? "" : "#" + root["id"].toString())
                           + " " + root["cls"].toString().split(" ").first());
        rootItem->setText(2, root["text"].toString());
        rootItem->setData(0, Qt::UserRole, result);

        buildTree(root["children"].toArray(), rootItem);
        rootItem->setExpanded(true);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::buildTree(const QJsonArray &nodes, QTreeWidgetItem *parent) {
    for (const auto &val : nodes) {
        QJsonObject obj = val.toObject();

        auto *item = new QTreeWidgetItem(parent);
        item->setText(0, "<" + obj["tag"].toString() + ">");

        QString badge;
        if (!obj["id"].toString().isEmpty())
            badge = "#" + obj["id"].toString();
        else if (!obj["cls"].toString().isEmpty())
            badge = "." + obj["cls"].toString().split(" ").first();
        item->setText(1, badge);
        item->setText(2, obj["text"].toString());
        item->setData(0, Qt::UserRole, val.toVariant());

        QString tag = obj["tag"].toString();
        if (tag == "div" || tag == "section" || tag == "article")
            item->setForeground(0, QColor("#88ccff"));
        else if (tag == "a")
            item->setForeground(0, QColor("#ffaa44"));
        else if (tag.startsWith("h"))
            item->setForeground(0, QColor("#ff6688"));
        else if (tag == "input" || tag == "form" || tag == "button")
            item->setForeground(0, QColor("#00cc66"));
        else if (tag == "script" || tag == "style")
            item->setForeground(0, QColor("#555555"));

        buildTree(obj["children"].toArray(), item);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::onTreeItemClicked(QTreeWidgetItem *item, int /*column*/) {
    if (!item) return;

    QVariant data = item->data(0, Qt::UserRole);
    QJsonDocument doc = QJsonDocument::fromVariant(data);
    if (doc.isNull()) return;
    QJsonObject obj = doc.object();

    QString info;
    info += "TAG:       " + obj["tag"].toString()  + "\n";
    info += "ID:        " + obj["id"].toString()   + "\n";
    info += "CLASS:     " + obj["cls"].toString()  + "\n";
    info += "TEXT:      " + obj["text"].toString() + "\n";
    info += "SELECTOR:  " + obj["selector"].toString() + "\n\n";
    info += "ATTRIBUTES:\n";
    QJsonObject attrs = obj["attrs"].toObject();
    for (auto it = attrs.begin(); it != attrs.end(); ++it)
        info += "  " + it.key() + " = " + it.value().toString() + "\n";

    m_infoPanel->setPlainText(info);

    QString sel = obj["selector"].toString();
    if (!sel.isEmpty())
        m_page->runJavaScript(highlightJS(sel));
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::onSelectorChanged(const QString &text) {
    if (text.trimmed().isEmpty()) {
        m_page->runJavaScript(clearHighlightJS());
        return;
    }
    m_page->runJavaScript(highlightJS(text.trimmed()));
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::onExportClicked() {
    auto *item = m_tree->currentItem();
    if (!item) {
        QMessageBox::information(this, "Piggy Tab",
            "Select an element in the tree first.");
        return;
    }

    QVariant data = item->data(0, Qt::UserRole);
    QJsonDocument doc = QJsonDocument::fromVariant(data);
    QString json = doc.toJson(QJsonDocument::Indented);

    QApplication::clipboard()->setText(json);

    QString path = QFileDialog::getSaveFileName(
        this, "Save Element", QDir::homePath() + "/piggy_export.json",
        "JSON (*.json);;All Files (*)");
    if (!path.isEmpty()) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            f.write(json.toUtf8());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyTab::pollDomChanges() {
    if (!m_page) return;
    m_page->runJavaScript(
        "document.querySelectorAll('*').length",
        [this](const QVariant &result) {
            static int lastCount = -1;
            int count = result.toInt();
            if (count != lastCount) {
                lastCount = count;
                injectDomExtractor();
                // Re-inject click listener after DOM rebuilds
                injectClickListener();
                if (m_inspectModeActive)
                    injectHoverTooltip();
            }
        }
    );
}