#include "DevToolsPanel.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// ═════════════════════════════════════════════════════════════════════════════
//  Actions
// ═════════════════════════════════════════════════════════════════════════════
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

// ═════════════════════════════════════════════════════════════════════════════
//  Session export / import
// ═════════════════════════════════════════════════════════════════════════════
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