#include "DevToolsPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QScrollArea>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QScrollBar>

// ═════════════════════════════════════════════════════════════════════════════
//  WelcomeScreen  (T&C splash)
// ═════════════════════════════════════════════════════════════════════════════
WelcomeScreen::WelcomeScreen(QWidget *parent) : QWidget(parent) {
    setStyleSheet("background:#080808;");
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        "QScrollArea { border:none; background:#080808; }"
        "QScrollBar:vertical { background:#0a0a0a; width:6px; border:none; }"
        "QScrollBar::handle:vertical { background:#1e1e1e; border-radius:3px; min-height:20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
    );

    auto *content = new QWidget;
    content->setStyleSheet("background:#080808;");
    auto *cl = new QVBoxLayout(content);
    cl->setContentsMargins(80, 60, 80, 40);
    cl->setSpacing(0);

    auto makeLabel = [&](const QString &text, int size, const QString &color,
                         bool bold = false) {
        auto *l = new QLabel(text, content);
        l->setWordWrap(true);
        l->setStyleSheet(QString(
            "color:%1; font-family:monospace; font-size:%2px; %3 background:transparent;")
            .arg(color).arg(size).arg(bold ? "font-weight:bold;" : ""));
        return l;
    };

    auto makeSep = [&]() {
        auto *s = new QFrame(content);
        s->setFrameShape(QFrame::HLine);
        s->setStyleSheet("border:none; background:#1a1a1a; max-height:1px; margin:20px 0;");
        return s;
    };

    auto *logo = new QLabel(content);
    logo->setText(R"(
<pre style="color:#00cc66; font-family:monospace; font-size:13px; background:transparent;">
 ███╗   ██╗ ██████╗ ████████╗██╗  ██╗██╗███╗   ██╗ ██████╗
 ████╗  ██║██╔═══██╗╚══██╔══╝██║  ██║██║████╗  ██║██╔════╝
 ██╔██╗ ██║██║   ██║   ██║   ███████║██║██╔██╗ ██║██║  ███╗
 ██║╚██╗██║██║   ██║   ██║   ██╔══██║██║██║╚██╗██║██║   ██║
 ██║ ╚████║╚██████╔╝   ██║   ██║  ██║██║██║ ╚████║╚██████╔╝
 ╚═╝  ╚═══╝ ╚═════╝    ╚═╝   ╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝ ╚═════╝
                    B R O W S E R  v0.1
</pre>)");
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet("background:transparent;");

    cl->addWidget(logo);
    cl->addSpacing(10);
    cl->addWidget(makeLabel("Does nothing... except everything that matters.",
                            14, "#00cc66", true));
    cl->addSpacing(6);
    cl->addWidget(makeLabel(
        "Before you proceed, read this. It is short, honest, and important.",
        12, "#666"));
    cl->addWidget(makeSep());

    cl->addWidget(makeLabel("WHAT NOTHING BROWSER IS", 13, "#00cc66", true));
    cl->addSpacing(10);

    QStringList advantages = {
        "⬡  Full network inspector built in — see every request, response, WebSocket frame, cookie, and storage write as it happens.",
        "⬡  One-click export to Python, cURL, JavaScript or raw HTTP — reverse any API in minutes.",
        "⬡  Fingerprint spoofing at engine level — randomised Chrome UA, hardware concurrency, device memory, screen resolution, WebGL vendor.",
        "⬡  Zero telemetry. Zero analytics. Zero phoning home. We literally capture nothing about you.",
        "⬡  No history saved. No bookmarks synced. No passwords stored. Every session is disposable.",
        "⬡  Designed for scrapers, automation engineers, API researchers, and people who hate black boxes.",
        "⬡  70 MB base RAM target — run many instances without your machine crying.",
        "⬡  Downloadable captures — save any request, response, or WebSocket session to a file.",
    };
    for (auto &a : advantages) { cl->addSpacing(6); cl->addWidget(makeLabel(a, 12, "#aaaaaa")); }

    cl->addWidget(makeSep());
    cl->addWidget(makeLabel("WHAT NOTHING BROWSER IS NOT", 13, "#ff4444", true));
    cl->addSpacing(10);

    QStringList sideEffects = {
        "✕  Not a daily driver. Banking sites, Google properties, and Facebook will block or degrade you. This is expected.",
        "✕  Not a Chrome extension host. Extensions will not load.",
        "✕  Not anonymous. Fingerprint spoofing reduces entropy — it does not make you invisible. Use a VPN separately.",
        "✕  Not a captcha silver bullet in v0.1. The captcha solver is planned for v0.3.",
        "✕  Not a password manager. Store nothing sensitive in this browser.",
        "✕  Not production-hardened yet. You will find bugs. Report them.",
    };
    for (auto &s : sideEffects) { cl->addSpacing(6); cl->addWidget(makeLabel(s, 12, "#888888")); }

    cl->addWidget(makeSep());
    cl->addWidget(makeLabel("COMING IN LATER VERSIONS", 13, "#0088ff", true));
    cl->addSpacing(10);

    QStringList roadmap = {
        "→  v0.2 — Windows support, improved WS frame display, response body search",
        "→  v0.3 — Built-in captcha solver (reCAPTCHA v2/v3, hCaptcha)",
        "→  v0.4 — Script marketplace integration, headless mode",
        "→  v0.5 — Multi-tab support, session profiles, proxy manager",
        "→  v1.0 — Stable release, open-source UI layer, enterprise API",
    };
    for (auto &r : roadmap) { cl->addSpacing(6); cl->addWidget(makeLabel(r, 12, "#555555")); }

    cl->addWidget(makeSep());
    cl->addWidget(makeLabel("BY USING THIS SOFTWARE YOU UNDERSTAND:", 12, "#444", true));
    cl->addSpacing(8);
    cl->addWidget(makeLabel(
        "This tool is provided for legitimate network research, API reverse engineering, "
        "web scraping for lawful purposes, and personal automation. You are responsible "
        "for how you use it. The authors accept no liability for misuse. Using this tool "
        "to violate a website's terms of service is your decision and your consequence.",
        11, "#333"));

    cl->addSpacing(40);

    auto *scrollHint = new QLabel("↓  Scroll to the bottom to continue  ↓", content);
    scrollHint->setAlignment(Qt::AlignCenter);
    scrollHint->setStyleSheet(
        "color:#333; font-family:monospace; font-size:11px; background:transparent;");
    cl->addWidget(scrollHint);
    cl->addSpacing(20);

    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    auto *bottomBar = new QWidget(this);
    bottomBar->setFixedHeight(54);
    bottomBar->setStyleSheet("background:#0a0a0a; border-top:1px solid #1a1a1a;");
    auto *bl = new QHBoxLayout(bottomBar);
    bl->setContentsMargins(40, 8, 40, 8);

    auto *hint = new QLabel("Scroll through the full list above, then click to proceed.", bottomBar);
    hint->setStyleSheet("color:#333; font-family:monospace; font-size:11px;");

    auto *acceptBtn = new QPushButton("I UNDERSTAND — OPEN NOTHING BROWSER", bottomBar);
    acceptBtn->setFixedHeight(36);
    acceptBtn->setStyleSheet(R"(
        QPushButton {
            background:#001a00; color:#00cc66;
            border:1px solid #00cc66; border-radius:3px;
            font-family:monospace; font-size:12px; font-weight:bold;
            padding:0 24px;
        }
        QPushButton:hover { background:#002a00; }
        QPushButton:pressed { background:#003300; }
    )");

    connect(acceptBtn, &QPushButton::clicked, this, &WelcomeScreen::accepted);

    acceptBtn->setEnabled(false);
    acceptBtn->setStyleSheet(acceptBtn->styleSheet() +
        "QPushButton:disabled { color:#1a3a1a; border-color:#1a3a1a; background:#050505; }");

    connect(scroll->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [scroll, acceptBtn](int val) {
        int max = scroll->verticalScrollBar()->maximum();
        if (max > 0 && val >= max - 40)
            acceptBtn->setEnabled(true);
    });

    bl->addWidget(hint, 1);
    bl->addWidget(acceptBtn);
    root->addWidget(bottomBar);
}

// ═════════════════════════════════════════════════════════════════════════════
//  DevToolsPanel helpers
// ═════════════════════════════════════════════════════════════════════════════
void DevToolsPanel::clip(const QString &text) {
    QGuiApplication::clipboard()->setText(text);
}

QPushButton *DevToolsPanel::btn(const QString &label, const QString &color, QWidget *parent) {
    auto *b = new QPushButton(label, parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QString(R"(
        QPushButton {
            background:#0d0d0d; color:%1;
            border:1px solid %1; border-radius:2px;
            font-family:monospace; font-size:10px;
            padding:3px 10px; min-width:80px;
        }
        QPushButton:hover  { background:%2; }
        QPushButton:pressed{ background:%3; }
    )").arg(color)
       .arg(color == "#00cc66" ? "#001800" :
            color == "#0088ff" ? "#001020" :
            color == "#ff4444" ? "#1a0000" : "#161616")
       .arg(color == "#00cc66" ? "#002a00" :
            color == "#0088ff" ? "#001830" :
            color == "#ff4444" ? "#2a0000" : "#222"));
    return b;
}

QString DevToolsPanel::panelStyle() {
    return R"(
        QWidget   { background:#0d0d0d; color:#cccccc; }
        QTabWidget::pane { border:none; background:#0d0d0d; }
        QTabBar::tab {
            background:#111; color:#555; padding:5px 14px;
            border:none; font-family:monospace; font-size:11px;
            border-right:1px solid #1a1a1a;
        }
        QTabBar::tab:selected { background:#0d0d0d; color:#00cc66;
                                border-bottom:2px solid #00cc66; }
        QTabBar::tab:hover { color:#aaa; }
        QTableWidget {
            background:#0d0d0d; color:#cccccc;
            font-family:monospace; font-size:11px;
            border:none; gridline-color:#161616;
            selection-background-color:#0a2a0a;
        }
        QTableWidget::item { padding:2px 6px; border-bottom:1px solid #141414; }
        QHeaderView::section {
            background:#090909; color:#3a3a3a;
            font-family:monospace; font-size:10px;
            border:none; border-right:1px solid #1a1a1a;
            border-bottom:1px solid #1e1e1e; padding:3px 6px;
        }
        QTextEdit {
            background:#080808; color:#00ff88;
            font-family:monospace; font-size:11px; border:none;
        }
        QLineEdit {
            background:#111; color:#ccc; border:1px solid #222;
            border-radius:2px; padding:3px 8px;
            font-family:monospace; font-size:11px;
        }
        QLineEdit:focus { border-color:#00cc66; }
        QComboBox {
            background:#111; color:#888; border:1px solid #222;
            border-radius:2px; padding:3px 8px;
            font-family:monospace; font-size:11px;
        }
        QComboBox QAbstractItemView {
            background:#111; color:#ccc; selection-background-color:#0a2a0a;
        }
        QComboBox::drop-down { border:none; }
        QSplitter::handle { background:#181818; width:1px; height:1px; }
        QLabel#cnt { color:#00cc66; font-family:monospace;
                     font-size:11px; font-weight:bold; }
        QScrollBar:vertical { background:#090909; width:5px; border:none; }
        QScrollBar::handle:vertical { background:#1e1e1e; border-radius:2px; min-height:16px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
    )";
}

QTableWidgetItem *DevToolsPanel::makeItem(const QString &text, const QColor &color) {
    auto *it = new QTableWidgetItem(text);
    it->setForeground(color);
    it->setFlags(it->flags() & ~Qt::ItemIsEditable);
    return it;
}

void DevToolsPanel::updateTabLabel(int idx, const QString &name, int count) {
    m_tabs->setTabText(idx, QString("%1 [%2]").arg(name).arg(count));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cookie helper — build "name=val; name2=val2" for a given request URL
// ─────────────────────────────────────────────────────────────────────────────
QString DevToolsPanel::cookiesForUrl(const QString &url) const {
    QUrl u(url);
    QString host = u.host(); // e.g. "comm.heloepunditsskyhook.cfd"
    QString out;
    for (auto &ce : m_cookieEntries) {
        QString dom = ce.cookie.domain;
        // domain can be ".example.com" or "example.com"
        QString cleanDom = dom.startsWith('.') ? dom.mid(1) : dom;
        if (host.endsWith(cleanDom) || host == cleanDom)
            out += ce.cookie.name + "=" + ce.cookie.value + "; ";
    }
    out = out.trimmed();
    if (out.endsWith(';')) out.chop(1);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Firefox-style formatters
// ─────────────────────────────────────────────────────────────────────────────
QString DevToolsPanel::buildSummaryBlock(const NetEntry &e) const {
    QUrl u(e.url);
    QString out;
    out += QString("%1 %2 %3\n\n")
               .arg(e.method)
               .arg(u.path().isEmpty() ? "/" : u.path())
               .arg(e.status.isEmpty() ? "—" : e.status);

    auto row = [](const QString &k, const QString &v) {
        return QString("%1%2\n")
            .arg(k.leftJustified(26, ' '))
            .arg(v);
    };

    out += row("Scheme",          u.scheme());
    out += row("Host",            u.host());
    out += row("Filename",        u.path());

    // ── Query string: full raw + decoded params, all selectable ──────────────
    if (!u.query().isEmpty()) {
        out += "\nQuery string (raw)\n";
        out += "    " + u.query() + "\n";
        out += "\nQuery params (decoded)\n";
        QUrlQuery q(u);
        const auto items = q.queryItems(QUrl::FullyDecoded);
        for (auto &pair : items)
            out += QString("    %1  =  %2\n")
                       .arg(pair.first.leftJustified(32, ' '))
                       .arg(pair.second);
        out += "\n";
    }

    out += row("Status",          e.status.isEmpty() ? "—" : e.status);
    out += row("MIME type",       e.mime.isEmpty()   ? "—" : e.mime);
    out += row("Referrer Policy", "strict-origin-when-cross-origin");
    out += row("DNS Resolution",  "System");

    // ── Full URL at bottom — always selectable ────────────────────────────────
    out += "\nFull URL\n    " + e.url + "\n";

    return out;
}

QString DevToolsPanel::buildHeadersBlock(const NetEntry &e) const {
    QString out;
    out += "\n── Request Headers ──────────────────────────────────────\n";
    QJsonDocument doc = QJsonDocument::fromJson(e.reqHeaders.toUtf8());
    if (doc.isObject()) {
        auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            out += QString("%1\n    %2\n").arg(it.key()).arg(it.value().toString());
    } else if (!e.reqHeaders.isEmpty()) {
        out += e.reqHeaders + "\n";
    } else {
        out += "(none captured at JS layer)\n";
    }
    out += "\n── Response Headers ─────────────────────────────────────\n";
    out += e.resHeaders.isEmpty()
        ? "(not available — response headers only captured for XHR/fetch)\n"
        : e.resHeaders + "\n";
    return out;
}

QString DevToolsPanel::buildRaw(const NetEntry &e) const {
    QUrl u(e.url);
    QString out;
    out += QString("%1 %2 HTTP/1.1\n").arg(e.method)
               .arg(u.path().isEmpty() ? "/" : u.path());
    if (!u.query().isEmpty())
        out += "    ?" + u.query() + "\n";
    out += QString("Host: %1\n").arg(u.host());
    QJsonDocument doc = QJsonDocument::fromJson(e.reqHeaders.toUtf8());
    if (doc.isObject()) {
        auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            out += QString("%1: %2\n").arg(it.key()).arg(it.value().toString());
    }
    if (!e.body.isEmpty()) {
        out += "\n";
        out += e.body;
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Code generators — now include body + cookies
// ─────────────────────────────────────────────────────────────────────────────
QString DevToolsPanel::generatePython(const QString &method, const QString &url,
                                       const QString &headers, const QString &body,
                                       const QString &cookies) {
    // ── Headers dict ──────────────────────────────────────────────────────────
    QString hdrs = "{\n";
    QJsonDocument doc = QJsonDocument::fromJson(headers.toUtf8());
    if (doc.isObject()) {
        auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            hdrs += QString("    \"%1\": \"%2\",\n")
                        .arg(it.key())
                        .arg(it.value().toString().replace("\\", "\\\\").replace("\"", "\\\""));
    } else {
        hdrs += "    \"User-Agent\": \"Mozilla/5.0 (X11; Linux x86_64) Chrome/120.0.0.0 Safari/537.36\",\n"
                "    \"Accept-Language\": \"en-US,en;q=0.9\",\n";
    }
    hdrs += "}";

    // ── Cookies dict ──────────────────────────────────────────────────────────
    QString cookieBlock = "{}";
    if (!cookies.isEmpty()) {
        cookieBlock = "{\n";
        for (const QString &part : cookies.split(';')) {
            QString trimmed = part.trimmed();
            int eq = trimmed.indexOf('=');
            if (eq > 0) {
                QString k = trimmed.left(eq).trimmed();
                QString v = trimmed.mid(eq + 1).trimmed();
                cookieBlock += QString("    \"%1\": \"%2\",\n")
                                   .arg(k)
                                   .arg(v.replace("\\", "\\\\").replace("\"", "\\\""));
            }
        }
        cookieBlock += "}";
    }

    // ── Body ──────────────────────────────────────────────────────────────────
    QString bodyLines;
    QString dataArg;
    if (!body.isEmpty()) {
        QJsonDocument bdoc = QJsonDocument::fromJson(body.toUtf8());
        if (!bdoc.isNull()) {
            bodyLines = QString("\ndata = %1\n").arg(QString(bdoc.toJson(QJsonDocument::Indented)));
            dataArg   = ", json=data";
        } else {
            // Raw string body (form-encoded, xml, etc.)
            QString escaped = body;
            escaped.replace("\\", "\\\\").replace("\"", "\\\"");
            bodyLines = QString("\ndata = \"%1\"\n").arg(escaped);
            dataArg   = ", data=data";
        }
    }

    return QString(
        "import requests\n\n"
        "url = \"%1\"\n\n"
        "headers = %2\n\n"
        "cookies = %3\n"
        "%4\n"
        "response = requests.%5(url, headers=headers, cookies=cookies%6)\n"
        "print(response.status_code)\n"
        "print(response.text[:2000])\n"
    ).arg(url, hdrs, cookieBlock, bodyLines, method.toLower(), dataArg);
}

QString DevToolsPanel::generateCurl(const QString &method, const QString &url,
                                     const QString &headers, const QString &body,
                                     const QString &cookies) {
    QString out = QString("curl -X %1 \\\n  '%2'").arg(method).arg(url);

    // ── Headers ───────────────────────────────────────────────────────────────
    QJsonDocument doc = QJsonDocument::fromJson(headers.toUtf8());
    if (doc.isObject()) {
        auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            out += QString(" \\\n  -H '%1: %2'").arg(it.key()).arg(it.value().toString());
    }

    // ── Cookies ───────────────────────────────────────────────────────────────
    if (!cookies.isEmpty())
        out += QString(" \\\n  -H 'Cookie: %1'").arg(cookies);

    // ── Body ──────────────────────────────────────────────────────────────────
    if (!body.isEmpty()) {
        QString escaped = body;
        escaped.replace("'", "'\\''"); // shell-escape single quotes
        out += QString(" \\\n  --data-raw '%1'").arg(escaped);
    }

    out += " \\\n  --compressed\n";
    return out;
}

QString DevToolsPanel::generateJS(const QString &method, const QString &url,
                                   const QString &headers, const QString &body) {
    // ── Headers object ────────────────────────────────────────────────────────
    QString hdrsBlock = "  headers: {\n";
    QJsonDocument doc = QJsonDocument::fromJson(headers.toUtf8());
    if (doc.isObject()) {
        auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            hdrsBlock += QString("    '%1': '%2',\n").arg(it.key()).arg(it.value().toString());
    } else {
        hdrsBlock +=
            "    'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) Chrome/120.0.0.0 Safari/537.36',\n"
            "    'Accept-Language': 'en-US,en;q=0.9',\n";
    }
    hdrsBlock += "  }";

    // ── Body ──────────────────────────────────────────────────────────────────
    QString bodyBlock;
    if (!body.isEmpty()) {
        QJsonDocument bdoc = QJsonDocument::fromJson(body.toUtf8());
        if (!bdoc.isNull())
            bodyBlock = QString(",\n  body: JSON.stringify(%1)").arg(QString(bdoc.toJson(QJsonDocument::Compact)));
        else
            bodyBlock = QString(",\n  body: `%1`").arg(body);
    }

    return QString(
        "const response = await fetch('%1', {\n"
        "  method: '%2',\n"
        "%3"
        "%4\n"
        "});\n\n"
        "const data = await response.text();\n"
        "console.log(response.status, data.slice(0, 500));\n"
    ).arg(url, method.toUpper(), hdrsBlock, bodyBlock);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────
DevToolsPanel::DevToolsPanel(QWidget *parent) : QWidget(parent) {
    setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildNetworkTab(), "NETWORK [0]");
    m_tabs->addTab(buildWsTab(),      "WS [0]");
    m_tabs->addTab(buildCookiesTab(), "COOKIES [0]");
    m_tabs->addTab(buildStorageTab(), "STORAGE [0]");
    m_tabs->addTab(buildExportTab(),  "EXPORT");
    root->addWidget(m_tabs);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Network Tab
// ─────────────────────────────────────────────────────────────────────────────
QWidget *DevToolsPanel::buildNetworkTab() {
    auto *w = new QWidget; w->setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(w);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    // toolbar
    auto *bar = new QWidget; bar->setFixedHeight(34);
    bar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *bl = new QHBoxLayout(bar);
    bl->setContentsMargins(8,3,8,3); bl->setSpacing(6);
    m_netCount = new QLabel("CAPTURED: 0", bar); m_netCount->setObjectName("cnt");
    m_netFilter = new QLineEdit(bar); m_netFilter->setPlaceholderText("filter url...");
    m_netFilter->setFixedWidth(180);
    m_typeFilter = new QComboBox(bar);
    m_typeFilter->addItems({"All","XHR","Fetch","WS","Script","Doc","Img","Other"});
    m_typeFilter->setFixedWidth(75);
    auto *clearBtn = btn("CLEAR", "#ff4444", bar); clearBtn->setFixedWidth(55);
    connect(clearBtn, &QPushButton::clicked, this, &DevToolsPanel::clearAll);
    connect(m_netFilter,  &QLineEdit::textChanged, this, &DevToolsPanel::filterNetwork);
    connect(m_typeFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int){ filterNetwork(m_netFilter->text()); });
    bl->addWidget(m_netCount); bl->addStretch();
    bl->addWidget(new QLabel("filter:", bar));
    bl->addWidget(m_netFilter); bl->addWidget(m_typeFilter); bl->addWidget(clearBtn);

    auto *splitter = new QSplitter(Qt::Horizontal, w);

    m_netTable = new QTableWidget(0, 5, splitter);
    m_netTable->setHorizontalHeaderLabels({"MTH","STATUS","TYPE","SIZE","URL"});
    m_netTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_netTable->setColumnWidth(0,48); m_netTable->setColumnWidth(1,48);
    m_netTable->setColumnWidth(2,52); m_netTable->setColumnWidth(3,52);
    m_netTable->verticalHeader()->setVisible(false);
    m_netTable->setShowGrid(false);
    m_netTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_netTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_netTable, &QTableWidget::cellClicked, this, &DevToolsPanel::onNetworkRowSelected);

    // right panel
    auto *rp = new QWidget(splitter); rp->setStyleSheet(panelStyle());
    auto *rl = new QVBoxLayout(rp); rl->setContentsMargins(0,0,0,0); rl->setSpacing(0);

    // copy/export bar
    auto *copyBar = new QWidget; copyBar->setFixedHeight(32);
    copyBar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *cbl = new QHBoxLayout(copyBar); cbl->setContentsMargins(6,2,6,2); cbl->setSpacing(4);

    auto *bCpHdr  = btn("COPY HEADERS",  "#888888", copyBar);
    auto *bCpResp = btn("COPY RESPONSE", "#888888", copyBar);
    auto *bCurl   = btn("AS CURL",       "#0088ff", copyBar);
    auto *bPython = btn("AS PYTHON",     "#00cc66", copyBar);
    auto *bDl     = btn("DOWNLOAD",      "#ffaa00", copyBar);

    connect(bCpHdr, &QPushButton::clicked, this, [this](){
        int r = m_netTable->currentRow();
        if (r >= 0 && r < m_netEntries.size())
            clip(buildSummaryBlock(m_netEntries[r]) + buildHeadersBlock(m_netEntries[r]));
    });
    connect(bCpResp, &QPushButton::clicked, this, [this](){
        int r = m_netTable->currentRow();
        if (r >= 0 && r < m_netEntries.size()) clip(m_netEntries[r].body);
    });
    connect(bCurl, &QPushButton::clicked, this, [this](){
        int r = m_netTable->currentRow();
        if (r >= 0 && r < m_netEntries.size()) {
            auto &e = m_netEntries[r];
            clip(generateCurl(e.method, e.url, e.reqHeaders, e.body, cookiesForUrl(e.url)));
        }
    });
    connect(bPython, &QPushButton::clicked, this, [this](){
        int r = m_netTable->currentRow();
        if (r >= 0 && r < m_netEntries.size()) {
            auto &e = m_netEntries[r];
            clip(generatePython(e.method, e.url, e.reqHeaders, e.body, cookiesForUrl(e.url)));
        }
    });
    connect(bDl, &QPushButton::clicked, this, &DevToolsPanel::downloadSelected);

    cbl->addWidget(bCpHdr); cbl->addWidget(bCpResp);
    cbl->addSpacing(6);
    cbl->addWidget(bCurl); cbl->addWidget(bPython);
    cbl->addSpacing(6);
    cbl->addWidget(bDl); cbl->addStretch();

    // detail sub-tabs
    m_netDetailTabs = new QTabWidget(rp);
    m_netDetailTabs->setStyleSheet(
        "QTabBar::tab { padding:4px 10px; font-size:10px; background:#090909; color:#444; "
        "border:none; border-right:1px solid #1a1a1a; }"
        "QTabBar::tab:selected { color:#00cc66; border-bottom:1px solid #00cc66; "
        "background:#0d0d0d; }");
    m_netHeadersView  = new QTextEdit; m_netHeadersView->setReadOnly(true);
    m_netHeadersView->setPlaceholderText("// click a request");
    m_netResponseView = new QTextEdit; m_netResponseView->setReadOnly(true);
    m_netResponseView->setPlaceholderText("// click a request");
    m_netRawView      = new QTextEdit; m_netRawView->setReadOnly(true);
    m_netRawView->setPlaceholderText("// click a request");
    m_netDetailTabs->addTab(m_netHeadersView,  "Summary + Headers");
    m_netDetailTabs->addTab(m_netResponseView, "Response");
    m_netDetailTabs->addTab(m_netRawView,      "Raw");

    rl->addWidget(copyBar);
    rl->addWidget(m_netDetailTabs, 1);

    splitter->addWidget(m_netTable);
    splitter->addWidget(rp);
    splitter->setSizes({600, 500});

    root->addWidget(bar);
    root->addWidget(splitter, 1);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
//  WebSocket Tab
// ─────────────────────────────────────────────────────────────────────────────
QWidget *DevToolsPanel::buildWsTab() {
    auto *w = new QWidget; w->setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(w);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    auto *bar = new QWidget; bar->setFixedHeight(34);
    bar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *bl = new QHBoxLayout(bar);
    bl->setContentsMargins(8,3,8,3); bl->setSpacing(8);
    m_wsCount = new QLabel("FRAMES: 0", bar); m_wsCount->setObjectName("cnt");

    auto *testBtn = btn("TEST WS (echo)", "#ffaa00", bar);
    testBtn->setToolTip("Opens wss://echo.websocket.org — sends a ping so you can see WS frames here");
    connect(testBtn, &QPushButton::clicked, this, [this](){
        QMessageBox::information(this, "WS Test",
            "In the Browser tab, navigate to:\n\n"
            "  https://www.piesocket.com/websocket-tester\n\n"
            "That page opens a WebSocket to a demo server and sends/receives frames.\n"
            "You will see them appear in this WS tab immediately.\n\n"
            "Other good test sites:\n"
            "  https://socketsbay.com/test-websockets\n"
            "  https://websocket.org/tools/websocket-echo/");
    });

    bl->addWidget(m_wsCount); bl->addWidget(testBtn); bl->addStretch();

    auto *splitter = new QSplitter(Qt::Horizontal, w);

    m_wsTable = new QTableWidget(0, 4, splitter);
    m_wsTable->setHorizontalHeaderLabels({"DIR","SIZE","TIME","PREVIEW"});
    m_wsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_wsTable->setColumnWidth(0,70); m_wsTable->setColumnWidth(1,50); m_wsTable->setColumnWidth(2,72);
    m_wsTable->verticalHeader()->setVisible(false);
    m_wsTable->setShowGrid(false);
    m_wsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_wsTable, &QTableWidget::cellClicked, this, &DevToolsPanel::onWsRowSelected);

    auto *rp = new QWidget(splitter); rp->setStyleSheet(panelStyle());
    auto *rl = new QVBoxLayout(rp); rl->setContentsMargins(0,0,0,0); rl->setSpacing(0);

    auto *copyBar = new QWidget; copyBar->setFixedHeight(32);
    copyBar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *cbl = new QHBoxLayout(copyBar); cbl->setContentsMargins(6,2,6,2); cbl->setSpacing(4);
    auto *bCp = btn("COPY FRAME", "#888888", copyBar);
    auto *bDl = btn("DOWNLOAD",   "#ffaa00", copyBar);

    connect(bCp, &QPushButton::clicked, this,
            [this](){ clip(m_wsDetail->toPlainText()); });

    connect(bDl, &QPushButton::clicked, this, [this](){
        QString path = QFileDialog::getSaveFileName(
            this, "Save WS Frame", QDir::homePath() + "/ws_frame.txt",
            "Text Files (*.txt);;JSON (*.json);;All Files (*)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) return;
        QTextStream(&f) << m_wsDetail->toPlainText();
    });

    cbl->addWidget(bCp); cbl->addWidget(bDl); cbl->addStretch();

    m_wsDetail = new QTextEdit(rp);
    m_wsDetail->setReadOnly(true);
    m_wsDetail->setPlaceholderText("// select a frame\n\n// Tip: go to piesocket.com/websocket-tester in the Browser tab to see live WS frames");

    rl->addWidget(copyBar); rl->addWidget(m_wsDetail, 1);
    splitter->addWidget(m_wsTable); splitter->addWidget(rp);
    splitter->setSizes({500,600});

    root->addWidget(bar); root->addWidget(splitter,1);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cookies Tab
// ─────────────────────────────────────────────────────────────────────────────
QWidget *DevToolsPanel::buildCookiesTab() {
    auto *w = new QWidget; w->setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(w);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    auto *bar = new QWidget; bar->setFixedHeight(34);
    bar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *bl = new QHBoxLayout(bar); bl->setContentsMargins(8,3,8,3); bl->setSpacing(6);
    m_cookieCount = new QLabel("COOKIES: 0", bar); m_cookieCount->setObjectName("cnt");
    auto *copyAllBtn = btn("COPY ALL JSON", "#0088ff", bar);
    connect(copyAllBtn, &QPushButton::clicked, this, [this](){
        QString out = "{\n";
        for (auto &e : m_cookieEntries)
            out += QString("  \"%1\": \"%2\",\n").arg(e.cookie.name).arg(e.cookie.value);
        out += "}";
        clip(out);
    });
    bl->addWidget(m_cookieCount); bl->addStretch(); bl->addWidget(copyAllBtn);

    auto *splitter = new QSplitter(Qt::Vertical, w);

    m_cookieTable = new QTableWidget(0, 7, splitter);
    m_cookieTable->setHorizontalHeaderLabels({"NAME","VALUE","DOMAIN","PATH","HTTPONLY","SECURE","EXPIRES"});
    m_cookieTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_cookieTable->setColumnWidth(0,140); m_cookieTable->setColumnWidth(2,160);
    m_cookieTable->setColumnWidth(3,55);  m_cookieTable->setColumnWidth(4,68);
    m_cookieTable->setColumnWidth(5,55);  m_cookieTable->setColumnWidth(6,155);
    m_cookieTable->verticalHeader()->setVisible(false);
    m_cookieTable->setShowGrid(false);
    m_cookieTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cookieTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_cookieTable, &QTableWidget::cellClicked, this, &DevToolsPanel::onCookieRowSelected);

    auto *dp = new QWidget(splitter); dp->setStyleSheet(panelStyle());
    auto *dl = new QVBoxLayout(dp); dl->setContentsMargins(0,0,0,0); dl->setSpacing(0);

    auto *copyBar = new QWidget; copyBar->setFixedHeight(32);
    copyBar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *cbl = new QHBoxLayout(copyBar); cbl->setContentsMargins(6,2,6,2); cbl->setSpacing(4);
    auto *bAttr = btn("COPY COOKIE",  "#888888", copyBar);
    auto *bReq  = btn("COPY REQUEST", "#00cc66", copyBar);
    auto *bDl   = btn("DOWNLOAD",     "#ffaa00", copyBar);
    connect(bAttr, &QPushButton::clicked, this, [this](){ clip(m_cookieAttrView->toPlainText()); });
    connect(bReq,  &QPushButton::clicked, this, [this](){ clip(m_cookieRequestView->toPlainText()); });
    connect(bDl, &QPushButton::clicked, this, [this](){
        int row = m_cookieTable->currentRow();
        if (row < 0 || row >= m_cookieEntries.size()) return;
        QString name = m_cookieEntries[row].cookie.name;
        QString path = QFileDialog::getSaveFileName(
            this, "Save Cookie", QDir::homePath()+"/"+name+".txt",
            "Text Files (*.txt);;All Files (*)");
        if (path.isEmpty()) return;
        QFile f(path); if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) return;
        QTextStream(&f) << m_cookieAttrView->toPlainText()
                        << "\n\n" << m_cookieRequestView->toPlainText();
    });
    cbl->addWidget(bAttr); cbl->addWidget(bReq); cbl->addWidget(bDl); cbl->addStretch();

    m_cookieDetailTabs = new QTabWidget(dp);
    m_cookieDetailTabs->setStyleSheet(
        "QTabBar::tab { padding:4px 10px; font-size:10px; background:#090909; color:#444; "
        "border:none; border-right:1px solid #1a1a1a; }"
        "QTabBar::tab:selected { color:#00cc66; border-bottom:1px solid #00cc66; background:#0d0d0d; }");
    m_cookieAttrView    = new QTextEdit; m_cookieAttrView->setReadOnly(true);
    m_cookieAttrView->setPlaceholderText("// click a cookie above");
    m_cookieRequestView = new QTextEdit; m_cookieRequestView->setReadOnly(true);
    m_cookieRequestView->setPlaceholderText("// request that set this cookie");
    m_cookieDetailTabs->addTab(m_cookieAttrView,    "Cookie Info");
    m_cookieDetailTabs->addTab(m_cookieRequestView, "Set-By Request");

    dl->addWidget(copyBar); dl->addWidget(m_cookieDetailTabs, 1);
    splitter->addWidget(m_cookieTable); splitter->addWidget(dp);
    splitter->setSizes({350, 250});

    root->addWidget(bar); root->addWidget(splitter, 1);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Storage Tab
// ─────────────────────────────────────────────────────────────────────────────
QWidget *DevToolsPanel::buildStorageTab() {
    auto *w = new QWidget; w->setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(w);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    auto *bar = new QWidget; bar->setFixedHeight(34);
    bar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *bl = new QHBoxLayout(bar); bl->setContentsMargins(8,3,8,3);
    m_storageCount = new QLabel("ENTRIES: 0", bar); m_storageCount->setObjectName("cnt");
    bl->addWidget(m_storageCount); bl->addStretch();

    auto *splitter = new QSplitter(Qt::Horizontal, w);

    m_storageTable = new QTableWidget(0, 4, splitter);
    m_storageTable->setHorizontalHeaderLabels({"TYPE","ORIGIN","KEY","VALUE"});
    m_storageTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_storageTable->setColumnWidth(0,100); m_storageTable->setColumnWidth(1,200);
    m_storageTable->setColumnWidth(2,160);
    m_storageTable->verticalHeader()->setVisible(false);
    m_storageTable->setShowGrid(false);
    m_storageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_storageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_storageTable, &QTableWidget::cellClicked, this, &DevToolsPanel::onStorageRowSelected);

    auto *rp = new QWidget(splitter); rp->setStyleSheet(panelStyle());
    auto *rl = new QVBoxLayout(rp); rl->setContentsMargins(0,0,0,0); rl->setSpacing(0);

    auto *copyBar = new QWidget; copyBar->setFixedHeight(32);
    copyBar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *cbl = new QHBoxLayout(copyBar); cbl->setContentsMargins(6,2,6,2); cbl->setSpacing(4);
    auto *bCp = btn("COPY VALUE", "#888888", copyBar);
    auto *bDl = btn("DOWNLOAD",   "#ffaa00", copyBar);
    connect(bCp, &QPushButton::clicked, this, [this](){ clip(m_storageDetail->toPlainText()); });
    connect(bDl, &QPushButton::clicked, this, [this](){
        QString path = QFileDialog::getSaveFileName(
            this, "Save Storage Entry", QDir::homePath()+"/storage_entry.json",
            "JSON Files (*.json);;Text Files (*.txt);;All Files (*)");
        if (path.isEmpty()) return;
        QFile f(path); if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) return;
        QTextStream(&f) << m_storageDetail->toPlainText();
    });
    cbl->addWidget(bCp); cbl->addWidget(bDl); cbl->addStretch();

    m_storageDetail = new QTextEdit(rp); m_storageDetail->setReadOnly(true);
    m_storageDetail->setPlaceholderText("// select an entry");

    rl->addWidget(copyBar); rl->addWidget(m_storageDetail,1);
    splitter->addWidget(m_storageTable); splitter->addWidget(rp);
    splitter->setSizes({600,500});

    root->addWidget(bar); root->addWidget(splitter,1);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Export Tab
// ─────────────────────────────────────────────────────────────────────────────
QWidget *DevToolsPanel::buildExportTab() {
    auto *w = new QWidget; w->setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(w);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    auto *bar = new QWidget; bar->setFixedHeight(34);
    bar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *bl = new QHBoxLayout(bar); bl->setContentsMargins(8,3,8,3); bl->setSpacing(6);
    m_exportFormat = new QComboBox(bar);
    m_exportFormat->addItems({"Python (requests)","Python (curl_cffi)",
                               "cURL","JavaScript (fetch)","Raw HTTP"});
    m_exportFormat->setFixedWidth(160);
    auto *genBtn  = btn("GENERATE", "#00cc66", bar);
    auto *copyBtn = btn("COPY",     "#0088ff", bar); copyBtn->setFixedWidth(60);
    auto *dlBtn   = btn("DOWNLOAD", "#ffaa00", bar); dlBtn->setFixedWidth(90);
    connect(genBtn,  &QPushButton::clicked, this, &DevToolsPanel::exportSelected);
    connect(copyBtn, &QPushButton::clicked, this, [this](){ clip(m_exportArea->toPlainText()); });
    connect(dlBtn,   &QPushButton::clicked, this, [this](){
        QString path = QFileDialog::getSaveFileName(
            this, "Save Export", QDir::homePath()+"/request_export.py",
            "Python (*.py);;Shell (*.sh);;JavaScript (*.js);;Text (*.txt);;All (*)");
        if (path.isEmpty()) return;
        QFile f(path); if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) return;
        QTextStream(&f) << m_exportArea->toPlainText();
    });
    bl->addWidget(m_exportFormat); bl->addWidget(genBtn);
    bl->addWidget(copyBtn); bl->addWidget(dlBtn); bl->addStretch();

    m_exportArea = new QTextEdit(w); m_exportArea->setReadOnly(true);
    m_exportArea->setPlaceholderText("# select a request in Network tab, then click GENERATE");
    root->addWidget(bar); root->addWidget(m_exportArea,1);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Data ingestion
// ─────────────────────────────────────────────────────────────────────────────
void DevToolsPanel::onRequestCaptured(const CapturedRequest &req) {
    if (!req.url.isEmpty()) {
        m_lastUrl = req.url; m_lastMethod = req.method; m_lastHeaders = req.requestHeaders;
    }
    QColor mc = req.method=="POST"   ? QColor("#ff8800")
              : req.method=="PUT"    ? QColor("#cc44ff")
              : req.method=="DELETE" ? QColor("#ff4444") : QColor("#00aaff");
    QColor sc = QColor("#444");
    if (!req.status.isEmpty()) {
        int c = req.status.toInt();
        sc = (c>=200&&c<300) ? QColor("#00cc66")
           : (c>=300&&c<400) ? QColor("#ffaa00")
           : (c>=400)        ? QColor("#ff4444") : QColor("#444");
    }
    QString sz = req.size > 0
        ? (req.size > 1024 ? QString::number(req.size/1024)+"k" : QString::number(req.size)+"b")
        : "-";
    int row = m_netTable->rowCount();
    m_netTable->insertRow(row); m_netTable->setRowHeight(row, 20);
    m_netTable->setItem(row,0,makeItem(req.method,mc));
    m_netTable->setItem(row,1,makeItem(req.status,sc));
    m_netTable->setItem(row,2,makeItem(req.type,QColor("#555")));
    m_netTable->setItem(row,3,makeItem(sz,QColor("#444")));
    m_netTable->setItem(row,4,makeItem(req.url));
    m_netEntries.append({req.method, req.url, req.status, req.type,
                         req.mimeType, req.requestBody, req.requestHeaders, req.responseHeaders,
                         req.responseBody});
    m_netTotal++;
    m_netCount->setText(QString("CAPTURED: %1").arg(m_netTotal));
    updateTabLabel(0, "NETWORK", m_netTotal);
    m_netTable->scrollToBottom();
}

void DevToolsPanel::onRawRequest(const QString &method, const QString &url,
                                  const QString &headers) {
    CapturedRequest req;
    req.method = method; req.url = url; req.type = "HTTP"; req.requestHeaders = headers;
    req.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    onRequestCaptured(req);
}

void DevToolsPanel::onWsFrame(const WebSocketFrame &frame) {
    QColor dc = frame.direction.contains("SENT") ? QColor("#ff8800")
              : frame.direction.contains("RECV") ? QColor("#00cc66")
              : frame.direction == "OPEN"        ? QColor("#0088ff")
                                                 : QColor("#444");

    QString preview;
    if (frame.isBinary && !frame.data.isEmpty() && !frame.data.startsWith("[")) {
        QByteArray raw = QByteArray::fromBase64(frame.data.toUtf8());
        QString hex;
        int previewBytes = qMin(raw.size(), 16);
        for (int i = 0; i < previewBytes; i++)
            hex += QString("%1 ").arg((unsigned char)raw[i], 2, 16, QChar('0'));
        preview = QString("[Binary %1b] %2...").arg(raw.size()).arg(hex.trimmed());
    } else {
        preview = frame.data.left(90).replace('\n', ' ');
    }

    int row = m_wsTable->rowCount();
    m_wsTable->insertRow(row);
    m_wsTable->setRowHeight(row, 20);
    m_wsTable->setItem(row, 0, makeItem(frame.direction, dc));
    m_wsTable->setItem(row, 1,
        makeItem(QString::number(frame.isBinary
            ? QByteArray::fromBase64(frame.data.toUtf8()).size()
            : frame.data.size()) + "b", QColor("#444")));
    m_wsTable->setItem(row, 2, makeItem(frame.timestamp, QColor("#333")));

    auto *it = makeItem(preview);
    it->setData(Qt::UserRole,   frame.data);
    it->setData(Qt::UserRole+1, frame.url);
    it->setData(Qt::UserRole+2, frame.isBinary);
    m_wsTable->setItem(row, 3, it);

    m_wsFrames.append(frame);
    m_wsTotal++;
    m_wsCount->setText(QString("FRAMES: %1").arg(m_wsTotal));
    updateTabLabel(1, "WS", m_wsTotal);
    m_wsTable->scrollToBottom();
}

void DevToolsPanel::onCookieCaptured(const CapturedCookie &c) {
    QString key = c.name + "@" + c.domain;
    int row;
    if (m_cookieRowMap.contains(key)) {
        row = m_cookieRowMap[key];
        m_cookieEntries[row].cookie = c;
    } else {
        row = m_cookieTable->rowCount();
        m_cookieTable->insertRow(row); m_cookieTable->setRowHeight(row, 20);
        m_cookieRowMap[key] = row;
        m_cookieEntries.append({c, m_lastUrl, m_lastMethod, m_lastHeaders});
        m_cookieTotal++;
    }
    QColor nc = c.httpOnly ? QColor("#ffaa00") : QColor("#00cc66");
    m_cookieTable->setItem(row,0,makeItem(c.name,nc));
    m_cookieTable->setItem(row,1,makeItem(c.value.left(60)));
    m_cookieTable->setItem(row,2,makeItem(c.domain,QColor("#777")));
    m_cookieTable->setItem(row,3,makeItem(c.path,QColor("#444")));
    m_cookieTable->setItem(row,4,makeItem(c.httpOnly?"YES":"",QColor("#ffaa00")));
    m_cookieTable->setItem(row,5,makeItem(c.secure?"YES":"",QColor("#0088ff")));
    m_cookieTable->setItem(row,6,makeItem(c.expires,QColor("#333")));
    m_cookieCount->setText(QString("COOKIES: %1").arg(m_cookieTotal));
    updateTabLabel(2, "COOKIES", m_cookieTotal);
}

void DevToolsPanel::onCookieRemoved(const QString &name, const QString &domain) {
    QString key = name + "@" + domain;
    if (!m_cookieRowMap.contains(key)) return;
    int row = m_cookieRowMap[key];
    for (int c = 0; c < m_cookieTable->columnCount(); c++)
        if (m_cookieTable->item(row, c))
            m_cookieTable->item(row, c)->setForeground(QColor("#2a2a2a"));
}

void DevToolsPanel::onStorageCaptured(const QString &origin, const QString &key,
                                       const QString &value, const QString &storageType) {
    QColor tc = storageType == "localStorage" ? QColor("#cc44ff") : QColor("#0088ff");
    int row = m_storageTable->rowCount();
    m_storageTable->insertRow(row); m_storageTable->setRowHeight(row, 20);
    m_storageTable->setItem(row,0,makeItem(storageType,tc));
    m_storageTable->setItem(row,1,makeItem(origin,QColor("#777")));
    m_storageTable->setItem(row,2,makeItem(key,QColor("#00cc66")));
    auto *it = makeItem(value.left(80));
    it->setData(Qt::UserRole, value);
    m_storageTable->setItem(row,3,it);
    m_storageTotal++;
    m_storageCount->setText(QString("ENTRIES: %1").arg(m_storageTotal));
    updateTabLabel(3, "STORAGE", m_storageTotal);
    m_storageTable->scrollToBottom();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Row selection
// ─────────────────────────────────────────────────────────────────────────────
void DevToolsPanel::onNetworkRowSelected(int row, int) {
    if (row < 0 || row >= m_netEntries.size()) return;
    const auto &e = m_netEntries[row];
    m_netHeadersView->setPlainText(buildSummaryBlock(e) + buildHeadersBlock(e));
    // Response tab — show server response body
    QJsonDocument doc = QJsonDocument::fromJson(e.responseBody.toUtf8());
    m_netResponseView->setPlainText(doc.isNull() ? e.responseBody : doc.toJson(QJsonDocument::Indented));
    m_netRawView->setPlainText(buildRaw(e));
}

void DevToolsPanel::onWsRowSelected(int row, int) {
    auto *it = m_wsTable->item(row, 3); if (!it) return;
    QString data     = it->data(Qt::UserRole).toString();
    QString url      = it->data(Qt::UserRole+1).toString();
    bool    isBinary = it->data(Qt::UserRole+2).toBool();

    QString out = "URL: " + url + "\n";
    out += "Direction: " + m_wsTable->item(row,0)->text() + "\n";
    out += "Time:      " + m_wsTable->item(row,2)->text() + "\n\n";

    if (isBinary && !data.isEmpty() && !data.startsWith("[")) {
        QByteArray raw = QByteArray::fromBase64(data.toUtf8());
        out += QString("── Binary Frame: %1 bytes ──────────────────\n\n").arg(raw.size());
        out += "── Hex Dump (first 256 bytes) ───────────────\n";
        QString hexLine, asciiLine;
        int limit = qMin(raw.size(), 256);
        for (int i = 0; i < limit; i++) {
            unsigned char b = (unsigned char)raw[i];
            hexLine  += QString("%1 ").arg(b, 2, 16, QChar('0'));
            asciiLine += (b >= 32 && b < 127) ? QChar(b) : QChar('.');
            if ((i+1) % 16 == 0) {
                out += QString("%1  %2  %3\n")
                           .arg(QString::number(i-15, 16).rightJustified(4,'0'))
                           .arg(hexLine.leftJustified(48))
                           .arg(asciiLine);
                hexLine.clear(); asciiLine.clear();
            }
        }
        if (!hexLine.isEmpty())
            out += QString("%1  %2  %3\n")
                       .arg(QString::number(limit - hexLine.count(' '), 16).rightJustified(4,'0'))
                       .arg(hexLine.leftJustified(48))
                       .arg(asciiLine);
        if (raw.size() > 256)
            out += QString("\n... %1 more bytes (use DOWNLOAD to get full frame)\n").arg(raw.size() - 256);
        m_wsDetail->setProperty("rawBase64", data);
    } else if (isBinary) {
        out += "[Binary frame — data capture incomplete]\n";
        out += "Reload the page with Nothing Browser open to capture properly\n";
    } else {
        out += "── Content ──────────────────────────────────\n";
        QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
        out += doc.isNull() ? data : doc.toJson(QJsonDocument::Indented);
    }

    m_wsDetail->setPlainText(out);
}

void DevToolsPanel::onCookieRowSelected(int row, int) {
    if (row < 0 || row >= m_cookieEntries.size()) return;
    const auto &e = m_cookieEntries[row];
    const auto &c = e.cookie;

    QString info;
    info += QString("Name:      %1\n").arg(c.name);
    info += QString("Value:     %1\n").arg(c.value);
    info += QString("Domain:    %1\n").arg(c.domain);
    info += QString("Path:      %1\n").arg(c.path);
    info += QString("HttpOnly:  %1\n").arg(c.httpOnly?"Yes":"No");
    info += QString("Secure:    %1\n").arg(c.secure?"Yes":"No");
    info += QString("Expires:   %1\n").arg(c.expires.isEmpty()?"Session":c.expires);
    info += "\n── Set-Cookie header ───────────────────────\n";
    info += QString("Set-Cookie: %1=%2; Domain=%3; Path=%4")
                .arg(c.name).arg(c.value).arg(c.domain).arg(c.path);
    if (c.httpOnly) info += "; HttpOnly";
    if (c.secure)   info += "; Secure";
    if (!c.expires.isEmpty()) info += "; Expires="+c.expires;
    info += "\n";
    m_cookieAttrView->setPlainText(info);

    QString reqText;
    if (e.setByUrl.isEmpty()) {
        reqText = "(request not captured — cookie may have been set before capture started)";
    } else {
        QUrl u(e.setByUrl);
        reqText += QString("%1 %2 HTTP/1.1\n")
                       .arg(e.setByMethod.isEmpty() ? "GET" : e.setByMethod)
                       .arg(u.path().isEmpty() ? "/" : u.path());
        reqText += QString("Host: %1\n").arg(u.host());
        QJsonDocument doc = QJsonDocument::fromJson(e.setByHeaders.toUtf8());
        if (doc.isObject()) {
            auto obj = doc.object();
            for (auto it = obj.begin(); it != obj.end(); ++it)
                reqText += QString("%1: %2\n").arg(it.key()).arg(it.value().toString());
        } else if (!e.setByHeaders.isEmpty()) {
            reqText += e.setByHeaders;
        }
        reqText += "\n── Full URL ────────────────────────────────\n" + e.setByUrl;
    }
    m_cookieRequestView->setPlainText(reqText);
}

void DevToolsPanel::onStorageRowSelected(int row, int) {
    auto *it = m_storageTable->item(row, 3); if (!it) return;
    QString full = it->data(Qt::UserRole).toString();
    QJsonDocument doc = QJsonDocument::fromJson(full.toUtf8());
    m_storageDetail->setPlainText(doc.isNull() ? full : doc.toJson(QJsonDocument::Indented));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Actions
// ─────────────────────────────────────────────────────────────────────────────
void DevToolsPanel::filterNetwork(const QString &text) {
    QString type = m_typeFilter->currentText();
    for (int r = 0; r < m_netTable->rowCount(); r++) {
        auto *urlIt  = m_netTable->item(r, 4);
        auto *typeIt = m_netTable->item(r, 2);
        if (!urlIt) continue;
        bool uOk = text.isEmpty() || urlIt->text().contains(text, Qt::CaseInsensitive);
        bool tOk = (type == "All") || (typeIt && typeIt->text() == type);
        m_netTable->setRowHidden(r, !(uOk && tOk));
    }
}

void DevToolsPanel::clearAll() {
    m_netTable->setRowCount(0); m_wsTable->setRowCount(0);
    m_cookieTable->setRowCount(0); m_storageTable->setRowCount(0);
    m_netHeadersView->clear(); m_netResponseView->clear(); m_netRawView->clear();
    m_wsDetail->clear(); m_cookieAttrView->clear(); m_cookieRequestView->clear();
    m_storageDetail->clear();
    m_netEntries.clear(); m_wsFrames.clear();
    m_cookieEntries.clear(); m_cookieRowMap.clear();
    m_netTotal = m_wsTotal = m_cookieTotal = m_storageTotal = 0;
    m_netCount->setText("CAPTURED: 0"); m_wsCount->setText("FRAMES: 0");
    m_cookieCount->setText("COOKIES: 0"); m_storageCount->setText("ENTRIES: 0");
    updateTabLabel(0,"NETWORK",0); updateTabLabel(1,"WS",0);
    updateTabLabel(2,"COOKIES",0); updateTabLabel(3,"STORAGE",0);
}

void DevToolsPanel::exportSelected() {
    int row = m_netTable->currentRow();
    if (row < 0 || row >= m_netEntries.size()) {
        m_exportArea->setPlainText("# select a request in the Network tab first"); return;
    }
    const auto &e = m_netEntries[row];
    QString cookieStr = cookiesForUrl(e.url);
    QString fmt = m_exportFormat->currentText();
    QString code;

    if (fmt.contains("curl_cffi")) {
        // Build headers for curl_cffi
        QString hdrs = "{\n";
        QJsonDocument doc = QJsonDocument::fromJson(e.reqHeaders.toUtf8());
        if (doc.isObject()) {
            auto obj = doc.object();
            for (auto it = obj.begin(); it != obj.end(); ++it)
                hdrs += QString("    \"%1\": \"%2\",\n").arg(it.key()).arg(it.value().toString());
        }
        hdrs += "}";

        QString cookieBlock = "{}";
        if (!cookieStr.isEmpty()) {
            cookieBlock = "{\n";
            for (const QString &part : cookieStr.split(';')) {
                QString trimmed = part.trimmed();
                int eq = trimmed.indexOf('=');
                if (eq > 0)
                    cookieBlock += QString("    \"%1\": \"%2\",\n")
                                       .arg(trimmed.left(eq).trimmed())
                                       .arg(trimmed.mid(eq+1).trimmed());
            }
            cookieBlock += "}";
        }

        QString bodyLines, dataArg;
        if (!e.body.isEmpty()) {
            QJsonDocument bdoc = QJsonDocument::fromJson(e.body.toUtf8());
            if (!bdoc.isNull()) {
                bodyLines = QString("\ndata = %1\n").arg(QString(bdoc.toJson(QJsonDocument::Indented)));
                dataArg = ", json=data";
            } else {
                bodyLines = QString("\ndata = \"%1\"\n").arg(e.body);
                dataArg = ", data=data";
            }
        }

        code = QString(
            "from curl_cffi import requests\n\n"
            "url = \"%1\"\n\n"
            "headers = %2\n\n"
            "cookies = %3\n"
            "%4\n"
            "response = requests.%5(url, headers=headers, cookies=cookies, impersonate=\"chrome120\"%6)\n"
            "print(response.status_code)\n"
            "print(response.text[:2000])\n"
        ).arg(e.url, hdrs, cookieBlock, bodyLines, e.method.toLower(), dataArg);
    }
    else if (fmt.contains("requests")) code = generatePython(e.method, e.url, e.reqHeaders, e.body, cookieStr);
    else if (fmt.contains("cURL"))     code = generateCurl(e.method, e.url, e.reqHeaders, e.body, cookieStr);
    else if (fmt.contains("fetch"))    code = generateJS(e.method, e.url, e.reqHeaders, e.body);
    else                               code = buildRaw(e);

    m_exportArea->setPlainText(code);
    m_tabs->setCurrentIndex(4);
}

void DevToolsPanel::downloadSelected() {
    int row = m_netTable->currentRow();
    if (row < 0 || row >= m_netEntries.size()) return;
    const auto &e = m_netEntries[row];

    QString safeHost = QUrl(e.url).host().replace(".", "-");
    QString defName  = QDir::homePath() + "/" + safeHost + "_request.txt";
    QString path = QFileDialog::getSaveFileName(
        this, "Download Request", defName,
        "Text Files (*.txt);;JSON (*.json);;All Files (*)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) {
        QMessageBox::warning(this, "Save Failed", "Could not write to:\n"+path); return;
    }
    QTextStream out(&f);
    out << buildSummaryBlock(e) << "\n";
    out << buildHeadersBlock(e) << "\n";
    if (!e.body.isEmpty()) {
        out << "── Request Body ─────────────────────────────\n";
        out << e.body << "\n\n";
    }
    if (!e.responseBody.isEmpty()) {
        out << "── Response Body ────────────────────────────\n";
        out << e.responseBody << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Session export / import
// ─────────────────────────────────────────────────────────────────────────────
bool DevToolsPanel::exportSession(const QString &path) {
    QJsonObject root;
    root["saved_at"]   = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    root["url"]        = m_currentUrl;
    root["nb_version"] = "0.1.0";

    QJsonArray netArr;
    for (auto &e : m_netEntries) {
        QJsonObject o;
        o["method"]     = e.method;
        o["url"]        = e.url;
        o["status"]     = e.status;
        o["type"]       = e.type;
        o["mime"]       = e.mime;
        o["reqHeaders"] = e.reqHeaders;
        o["resHeaders"] = e.resHeaders;
        o["body"]       = e.body;
        netArr.append(o);
    }
    root["captures"] = netArr;

    QJsonArray wsArr;
    for (auto &f : m_wsFrames) {
        QJsonObject o;
        o["url"]       = f.url;
        o["direction"] = f.direction;
        o["data"]      = f.data;
        o["binary"]    = f.isBinary;
        o["timestamp"] = f.timestamp;
        wsArr.append(o);
    }
    root["ws_frames"] = wsArr;

    QJsonArray cookieArr;
    for (auto &e : m_cookieEntries) {
        QJsonObject o;
        o["name"]     = e.cookie.name;
        o["value"]    = e.cookie.value;
        o["domain"]   = e.cookie.domain;
        o["path"]     = e.cookie.path;
        o["httpOnly"] = e.cookie.httpOnly;
        o["secure"]   = e.cookie.secure;
        o["expires"]  = e.cookie.expires;
        cookieArr.append(o);
    }
    root["cookies"] = cookieArr;

    QJsonObject storageObj, lsObj, ssObj;
    for (int r = 0; r < m_storageTable->rowCount(); r++) {
        auto *typeItem = m_storageTable->item(r, 0);
        auto *keyItem  = m_storageTable->item(r, 2);
        auto *valItem  = m_storageTable->item(r, 3);
        if (!typeItem || !keyItem || !valItem) continue;
        QString fullVal = valItem->data(Qt::UserRole).toString();
        if (typeItem->text() == "localStorage") lsObj[keyItem->text()] = fullVal;
        else                                    ssObj[keyItem->text()] = fullVal;
    }
    storageObj["localStorage"]   = lsObj;
    storageObj["sessionStorage"] = ssObj;
    root["storage"] = storageObj;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    m_lastSessionPath = path;
    return true;
}

bool DevToolsPanel::importSession(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;

    clearAll();

    QJsonObject root = doc.object();
    m_currentUrl = root["url"].toString();

    for (auto v : root["captures"].toArray()) {
        CapturedRequest req;
        QJsonObject o = v.toObject();
        req.method          = o["method"].toString();
        req.url             = o["url"].toString();
        req.status          = o["status"].toString();
        req.type            = o["type"].toString();
        req.mimeType        = o["mime"].toString();
        req.requestHeaders  = o["reqHeaders"].toString();
        req.responseHeaders = o["resHeaders"].toString();
        req.responseBody    = o["body"].toString();
        onRequestCaptured(req);
    }

    for (auto v : root["ws_frames"].toArray()) {
        WebSocketFrame fr;
        QJsonObject o = v.toObject();
        fr.url       = o["url"].toString();
        fr.direction = o["direction"].toString();
        fr.data      = o["data"].toString();
        fr.isBinary  = o["binary"].toBool();
        fr.timestamp = o["timestamp"].toString();
        onWsFrame(fr);
    }

    for (auto v : root["cookies"].toArray()) {
        CapturedCookie c;
        QJsonObject o = v.toObject();
        c.name     = o["name"].toString();
        c.value    = o["value"].toString();
        c.domain   = o["domain"].toString();
        c.path     = o["path"].toString();
        c.httpOnly = o["httpOnly"].toBool();
        c.secure   = o["secure"].toBool();
        c.expires  = o["expires"].toString();
        onCookieCaptured(c);
    }

    QJsonObject storage = root["storage"].toObject();
    auto restoreStorage = [&](const QJsonObject &obj, const QString &type) {
        for (auto it = obj.begin(); it != obj.end(); ++it)
            onStorageCaptured("(restored)", it.key(), it.value().toString(), type);
    };
    restoreStorage(storage["localStorage"].toObject(),   "localStorage");
    restoreStorage(storage["sessionStorage"].toObject(), "sessionStorage");

    m_lastSessionPath = path;
    return true;
}