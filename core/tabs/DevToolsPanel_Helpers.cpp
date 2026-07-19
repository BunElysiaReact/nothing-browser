#include "DevToolsPanel.h"
#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>

// ═════════════════════════════════════════════════════════════════════════════
//  UI helpers
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

// ═════════════════════════════════════════════════════════════════════════════
//  Cookie helper
// ═════════════════════════════════════════════════════════════════════════════
QString DevToolsPanel::cookiesForUrl(const QString &url) const {
    QUrl u(url);
    QString host = u.host();
    QString out;
    for (auto &ce : m_cookieEntries) {
        QString dom = ce.cookie.domain;
        QString cleanDom = dom.startsWith('.') ? dom.mid(1) : dom;
        if (host.endsWith(cleanDom) || host == cleanDom)
            out += ce.cookie.name + "=" + ce.cookie.value + "; ";
    }
    out = out.trimmed();
    if (out.endsWith(';')) out.chop(1);
    return out;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Firefox-style formatters
// ═════════════════════════════════════════════════════════════════════════════
QString DevToolsPanel::buildSummaryBlock(const NetEntry &e) const {
    QUrl u(e.url);
    QString out;
    out += QString("%1 %2 %3\n\n")
               .arg(e.method)
               .arg(u.path().isEmpty() ? "/" : u.path())
               .arg(e.status.isEmpty() ? "—" : e.status);

    auto row = [](const QString &k, const QString &v) {
        return QString("%1%2\n").arg(k.leftJustified(26, ' ')).arg(v);
    };

    out += row("Scheme",          u.scheme());
    out += row("Host",            u.host());
    out += row("Filename",        u.path());

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
    if (!e.body.isEmpty()) { out += "\n"; out += e.body; }
    return out;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Code generators
// ═════════════════════════════════════════════════════════════════════════════
QString DevToolsPanel::generatePython(const QString &method, const QString &url,
                                       const QString &headers, const QString &body,
                                       const QString &cookies) {
    QString hdrs = "{\n";
    QJsonDocument doc = QJsonDocument::fromJson(headers.toUtf8());
    if (doc.isObject()) {
        auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            hdrs += QString("    \"%1\": \"%2\",\n")
                        .arg(it.key())
                        .arg(it.value().toString().replace("\\","\\\\").replace("\"","\\\""));
    } else {
        hdrs += "    \"User-Agent\": \"Mozilla/5.0 (X11; Linux x86_64) Chrome/120.0.0.0 Safari/537.36\",\n"
                "    \"Accept-Language\": \"en-US,en;q=0.9\",\n";
    }
    hdrs += "}";

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
                                   .arg(k).arg(v.replace("\\","\\\\").replace("\"","\\\""));
            }
        }
        cookieBlock += "}";
    }

    QString bodyLines, dataArg;
    if (!body.isEmpty()) {
        QJsonDocument bdoc = QJsonDocument::fromJson(body.toUtf8());
        if (!bdoc.isNull()) {
            bodyLines = QString("\ndata = %1\n").arg(QString(bdoc.toJson(QJsonDocument::Indented)));
            dataArg   = ", json=data";
        } else {
            QString escaped = body;
            escaped.replace("\\","\\\\").replace("\"","\\\"");
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
    QJsonDocument doc = QJsonDocument::fromJson(headers.toUtf8());
    if (doc.isObject()) {
        auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            out += QString(" \\\n  -H '%1: %2'").arg(it.key()).arg(it.value().toString());
    }
    if (!cookies.isEmpty())
        out += QString(" \\\n  -H 'Cookie: %1'").arg(cookies);
    if (!body.isEmpty()) {
        QString escaped = body;
        escaped.replace("'", "'\\''");
        out += QString(" \\\n  --data-raw '%1'").arg(escaped);
    }
    out += " \\\n  --compressed\n";
    return out;
}

QString DevToolsPanel::generateJS(const QString &method, const QString &url,
                                   const QString &headers, const QString &body) {
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