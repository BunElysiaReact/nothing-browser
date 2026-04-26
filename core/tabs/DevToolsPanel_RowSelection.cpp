#include "DevToolsPanel.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

// ═════════════════════════════════════════════════════════════════════════════
//  Row selection handlers
// ═════════════════════════════════════════════════════════════════════════════
void DevToolsPanel::onNetworkRowSelected(int row, int) {
    if (row < 0 || row >= m_netEntries.size()) return;
    const auto &e = m_netEntries[row];
    m_netHeadersView->setPlainText(buildSummaryBlock(e) + buildHeadersBlock(e));
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