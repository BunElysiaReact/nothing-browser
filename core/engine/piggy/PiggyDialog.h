#pragma once
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>
#include <QMap>
#include <QObject>

class PiggyServer;
class QWebEnginePage;

// ─── Dialog + File Upload handling ───────────────────────────────────────────
//
//   dialog.accept   { tabId, text? }   — accept alert/confirm/prompt
//                                         text fills prompt dialogs
//   dialog.dismiss  { tabId }          — dismiss/cancel a dialog
//   dialog.status   { tabId }          — { pending:bool, type:string }
//   dialog.onDialog { tabId, action }  — pre-configure: "accept"|"dismiss"|"ignore"
//
//   upload          { selector, path } — set file input to file at path
//
// Dialog events emitted to all JS clients:
//   { type:"event", event:"dialog", tabId, dialogType:"alert|confirm|prompt|beforeunload",
//     message:"...", defaultValue:"..." }
//
// ─────────────────────────────────────────────────────────────────────────────

struct DialogState {
    bool    pending      = false;
    QString type;           // "alert" | "confirm" | "prompt" | "beforeunload"
    QString message;
    QString defaultValue;
    QString autoAction;     // "accept" | "dismiss" | "" (emit and wait)
};

class PiggyDialogHandler : public QObject {
    Q_OBJECT
public:
    explicit PiggyDialogHandler(PiggyServer *srv, QObject *parent = nullptr);

    void watchTab(const QString &tabId, QWebEnginePage *page);
    void unwatchTab(const QString &tabId);

    void acceptDialog(const QString &tabId, const QString &text = QString());
    void dismissDialog(const QString &tabId);
    void setAutoAction(const QString &tabId, const QString &action);

    DialogState dialogState(const QString &tabId) const { return m_states.value(tabId); }

    // Called by InterceptingPage overrides
    void onDialog(const QString &tabId, const QString &type,
                  const QString &message, const QString &defaultValue);
    bool onDialogConfirm(const QString &tabId, const QString &message);
    bool onDialogPrompt(const QString &tabId, const QString &message,
                        const QString &defaultValue, QString *result);

private:
    void broadcastEvent(const QJsonObject &event);

    PiggyServer                    *m_srv;
    QMap<QString, DialogState>      m_states;
    QMap<QString, QWebEnginePage*>  m_pages;
};

// ─── Free command handler ─────────────────────────────────────────────────────
bool piggy_handleDialog(PiggyServer *srv, const QString &c,
                        const QJsonObject &payload,
                        QLocalSocket *client, const QString &id,
                        const QString &tabId);

PiggyDialogHandler *piggy_dialogHandler();
void                piggy_dialogHandlerInit(PiggyServer *srv);