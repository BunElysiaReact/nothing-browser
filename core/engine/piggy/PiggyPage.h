#pragma once
#include <QWebEnginePage>
#include <QString>

// PiggyPage — a thin QWebEnginePage subclass whose only job is to intercept
// the dialog/file-picker hooks Qt only exposes as protected virtuals
// (javaScriptAlert/Confirm/Prompt, chooseFiles) and make their behavior
// controllable via the dialog.* / upload commands.
//
// Dialog auto-action modes:
//   "accept" (default) — confirm/prompt auto-accept, alert auto-dismisses
//   "dismiss"           — confirm/prompt auto-reject
//   "manual"            — the call blocks (via a nested QEventLoop) until
//                          dialog.accept / dialog.dismiss resolves it
//
// Upload: chooseFiles() is synchronous in Qt, so "upload" just needs to
// stash the desired file path before triggering the file input's click —
// no event loop needed there.

class PiggyPage : public QWebEnginePage {
    Q_OBJECT
public:
    explicit PiggyPage(QWebEngineProfile *profile, QObject *parent = nullptr);

    void    setAutoAction(const QString &action) { m_autoAction = action; }
    QString autoAction() const { return m_autoAction; }

    // Resolves a currently-pending manual dialog. No-op if nothing is pending.
    void resolvePending(bool accept, const QString &text = QString());

    bool    hasPending() const     { return m_dialogPending; }
    QString pendingType() const    { return m_dialogType; }
    QString pendingMessage() const { return m_dialogMessage; }

    // Sets the file path chooseFiles() will return on the next native
    // file-picker trigger. Cleared automatically after one use.
    void setPendingUploadPath(const QString &path) { m_pendingUploadPath = path; }

signals:
    // Fired the instant a dialog starts waiting (manual mode) or right as
    // it resolves (auto modes) — dialog.status / dialog.waitAndAccept use this.
    void dialogRequested(const QString &dialogType, const QString &message);
    void dialogResolved(const QString &dialogType);

protected:
    void javaScriptAlert(const QUrl &securityOrigin, const QString &msg) override;
    bool javaScriptConfirm(const QUrl &securityOrigin, const QString &msg) override;
    bool javaScriptPrompt(const QUrl &securityOrigin, const QString &msg,
                           const QString &defaultValue, QString *result) override;

    QStringList chooseFiles(QWebEnginePage::FileSelectionMode mode,
                             const QStringList &oldFiles,
                             const QStringList &acceptedMimeTypes) override;

private:
    // Blocks (via nested event loop) until resolvePending() is called.
    // Returns the accept/reject outcome; writes prompt text into
    // *promptResult if non-null.
    bool waitForManualResolution(const QString &type, const QString &msg,
                                  QString *promptResult);

    QString m_autoAction = "accept"; // "accept" | "dismiss" | "manual"

    bool    m_dialogPending = false;
    QString m_dialogType;
    QString m_dialogMessage;

    class QEventLoop *m_pendingLoop = nullptr;
    bool    m_resolveAccept = false;
    QString m_resolveText;

    QString m_pendingUploadPath;
};