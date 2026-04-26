#include "DevToolsPanel.h"
#include <QDateTime>
#include <QJsonDocument>

// ═════════════════════════════════════════════════════════════════════════════
//  Data ingestion
// ═════════════════════════════════════════════════════════════════════════════
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