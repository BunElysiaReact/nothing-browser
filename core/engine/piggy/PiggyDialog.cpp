#include "PiggyDialog.h"
#include "PiggyServer.h"
#include <QWebEnginePage>
#include <QJsonDocument>
#include <QFile>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── Singleton ────────────────────────────────────────────────────────────────
static PiggyDialogHandler *s_dialogHandler = nullptr;
PiggyDialogHandler *piggy_dialogHandler() { return s_dialogHandler; }
void piggy_dialogHandlerInit(PiggyServer *srv) {
    if (!s_dialogHandler) s_dialogHandler = new PiggyDialogHandler(srv, srv);
}

// ─── InterceptingPage ─────────────────────────────────────────────────────────
// Subclass of QWebEnginePage that overrides dialog methods.
// Created per-tab when dialog watching is needed.
class InterceptingPage : public QWebEnginePage {
public:
    QString tabId;
    PiggyDialogHandler *handler = nullptr;

    explicit InterceptingPage(QWebEngineProfile *profile,
                               const QString &tid,
                               PiggyDialogHandler *h,
                               QObject *parent = nullptr)
        : QWebEnginePage(profile, parent), tabId(tid), handler(h) {}

protected:
    void javaScriptAlert(const QUrl &, const QString &msg) override {
        if (!handler) return;
        handler->onDialog(tabId, "alert", msg, "");
    }

    bool javaScriptConfirm(const QUrl &, const QString &msg) override {
        if (!handler) return false;
        return handler->onDialogConfirm(tabId, msg);
    }

    bool javaScriptPrompt(const QUrl &, const QString &msg,
                          const QString &def, QString *result) override {
        if (!handler) return false;
        return handler->onDialogPrompt(tabId, msg, def, result);
    }
};

// ─── PiggyDialogHandler ───────────────────────────────────────────────────────

PiggyDialogHandler::PiggyDialogHandler(PiggyServer *srv, QObject *parent)
    : QObject(parent), m_srv(srv) {}

void PiggyDialogHandler::watchTab(const QString &tabId, QWebEnginePage *page) {
    m_pages[tabId]  = page;
    m_states[tabId] = DialogState{};
}

void PiggyDialogHandler::unwatchTab(const QString &tabId) {
    m_pages.remove(tabId);
    m_states.remove(tabId);
}

// Called by InterceptingPage overrides
void PiggyDialogHandler::onDialog(const QString &tabId,
                                   const QString &type,
                                   const QString &message,
                                   const QString &defaultValue)
{
    m_states[tabId].pending      = true;
    m_states[tabId].type         = type;
    m_states[tabId].message      = message;
    m_states[tabId].defaultValue = defaultValue;

    QJsonObject ev;
    ev["type"]         = "event";
    ev["event"]        = "dialog";
    ev["tabId"]        = tabId;
    ev["dialogType"]   = type;
    ev["message"]      = message;
    ev["defaultValue"] = defaultValue;
    broadcastEvent(ev);
}

bool PiggyDialogHandler::onDialogConfirm(const QString &tabId, const QString &message) {
    onDialog(tabId, "confirm", message, "");
    QString action = m_states[tabId].autoAction;
    m_states[tabId].pending = false;
    return action != "dismiss"; // true = accept
}

bool PiggyDialogHandler::onDialogPrompt(const QString &tabId, const QString &message,
                                        const QString &defaultValue, QString *result)
{
    onDialog(tabId, "prompt", message, defaultValue);
    QString action = m_states[tabId].autoAction;
    m_states[tabId].pending = false;
    if (action == "dismiss") return false;
    if (result) *result = m_states[tabId].defaultValue;
    return true;
}

void PiggyDialogHandler::acceptDialog(const QString &tabId, const QString &text) {
    if (!m_states.contains(tabId)) return;
    if (!text.isEmpty()) m_states[tabId].defaultValue = text;
    m_states[tabId].autoAction = "accept";
    m_states[tabId].pending    = false;
}

void PiggyDialogHandler::dismissDialog(const QString &tabId) {
    if (!m_states.contains(tabId)) return;
    m_states[tabId].autoAction = "dismiss";
    m_states[tabId].pending    = false;
}

void PiggyDialogHandler::setAutoAction(const QString &tabId, const QString &action) {
    if (!m_states.contains(tabId)) return;
    m_states[tabId].autoAction = action; // "accept" | "dismiss" | ""
}

void PiggyDialogHandler::broadcastEvent(const QJsonObject &event) {
    QByteArray msg = QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n";
    for (auto *client : m_srv->clients()) {
        if (client && client->state() == QLocalSocket::ConnectedState)
            client->write(msg);
    }
}

void PiggyDialogHandler::onJavaScriptConsoleMessage(
    int, const QString &, int, const QString &) {}

// ─── Command handler ──────────────────────────────────────────────────────────

bool piggy_handleDialog(PiggyServer *srv, const QString &c,
                        const QJsonObject &payload,
                        QLocalSocket *client, const QString &id,
                        const QString &tabId)
{
    auto *det = piggy_dialogHandler();

    if (c == "dialog.accept") {
        if (!det) { srv->respond(client, id, false, "dialog handler not initialized"); return true; }
        QString text = payload["text"].toString();
        det->acceptDialog(tabId, text);
        srv->respond(client, id, true, "dialog accepted");
        return true;
    }

    if (c == "dialog.dismiss") {
        if (!det) { srv->respond(client, id, false, "dialog handler not initialized"); return true; }
        det->dismissDialog(tabId);
        srv->respond(client, id, true, "dialog dismissed");
        return true;
    }

    if (c == "dialog.status") {
        if (!det) { srv->respond(client, id, true, QJsonObject{{"pending", false}}); return true; }
        DialogState s = det->dialogState(tabId);
        QJsonObject o;
        o["pending"]      = s.pending;
        o["type"]         = s.type;
        o["message"]      = s.message;
        o["defaultValue"] = s.defaultValue;
        srv->respond(client, id, true, o);
        return true;
    }

    if (c == "dialog.onDialog") {
        if (!det) { srv->respond(client, id, false, "dialog handler not initialized"); return true; }
        QString action = payload["action"].toString(); // "accept" | "dismiss" | ""
        det->setAutoAction(tabId, action);
        srv->respond(client, id, true, "dialog auto-action set: " + action);
        return true;
    }

    // ── upload ────────────────────────────────────────────────────────────────
    // Sets a file input element to a local file path via JS injection.
    // QWebEngine doesn't allow direct file input manipulation from JS
    // for security, so we use a DataTransfer trick for modern browsers
    // and fall back to the native QWebEnginePage file dialog override.
    if (c == "upload") {
        QString selector = payload["selector"].toString();
        QString filePath = payload["path"].toString();
        selector.replace("'", "\\'");

        if (!QFile::exists(filePath)) {
            srv->respond(client, id, false, "file not found: " + filePath);
            return true;
        }

        auto *p = piggy_page(srv, tabId);

        // Strategy: read file, create a Blob via base64, set on input via DataTransfer
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly)) {
            srv->respond(client, id, false, "cannot read file: " + filePath);
            return true;
        }
        QByteArray fileData = f.readAll().toBase64();
        QString base64 = QString::fromLatin1(fileData);
        QString fileName = QFileInfo(filePath).fileName();

        // Detect MIME type from extension
        QString ext = QFileInfo(filePath).suffix().toLower();
        QString mime = "application/octet-stream";
        if (ext == "pdf")  mime = "application/pdf";
        else if (ext == "png")  mime = "image/png";
        else if (ext == "jpg" || ext == "jpeg") mime = "image/jpeg";
        else if (ext == "gif")  mime = "image/gif";
        else if (ext == "txt")  mime = "text/plain";
        else if (ext == "csv")  mime = "text/csv";
        else if (ext == "json") mime = "application/json";

        QString js = QString(
            "(function(){"
            "var el = document.querySelector('%1');"
            "if (!el || el.type !== 'file') return false;"
            "var b64 = '%2';"
            "var mime = '%3';"
            "var name = '%4';"
            "var bytes = atob(b64);"
            "var arr = new Uint8Array(bytes.length);"
            "for (var i=0; i<bytes.length; i++) arr[i] = bytes.charCodeAt(i);"
            "var blob = new Blob([arr], { type: mime });"
            "var file = new File([blob], name, { type: mime });"
            "var dt = new DataTransfer();"
            "dt.items.add(file);"
            "el.files = dt.files;"
            "el.dispatchEvent(new Event('change', { bubbles: true }));"
            "el.dispatchEvent(new Event('input',  { bubbles: true }));"
            "return true;"
            "})()"
        ).arg(selector, base64, mime, fileName.replace("'", "\\'"));

        p->runJavaScript(js, [srv, client, id, fileName](const QVariant &r) {
            if (r.toBool())
                srv->respond(client, id, true, "file set: " + fileName);
            else
                srv->respond(client, id, false, "upload failed — element not found or not a file input");
        });
        return true;
    }

    return false;
}