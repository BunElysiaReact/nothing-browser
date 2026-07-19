#include "PiggyPage.h"
#include <QEventLoop>

PiggyPage::PiggyPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
}

// ─── Manual-resolution helper ─────────────────────────────────────────────────

bool PiggyPage::waitForManualResolution(const QString &type, const QString &msg,
                                         QString *promptResult) {
    m_dialogPending = true;
    m_dialogType    = type;
    m_dialogMessage = msg;
    emit dialogRequested(type, msg);

    QEventLoop loop;
    m_pendingLoop   = &loop;
    m_resolveAccept = false;
    m_resolveText.clear();
    loop.exec(); // blocks here until resolvePending() calls loop.quit()
    m_pendingLoop = nullptr;

    m_dialogPending = false;
    emit dialogResolved(type);

    if (promptResult) *promptResult = m_resolveText;
    return m_resolveAccept;
}

void PiggyPage::resolvePending(bool accept, const QString &text) {
    if (!m_dialogPending || !m_pendingLoop) return;
    m_resolveAccept = accept;
    m_resolveText   = text;
    m_pendingLoop->quit();
}

// ─── Dialog overrides ──────────────────────────────────────────────────────────

void PiggyPage::javaScriptAlert(const QUrl &securityOrigin, const QString &msg) {
    Q_UNUSED(securityOrigin);
    if (m_autoAction == "manual") {
        waitForManualResolution("alert", msg, nullptr);
        return;
    }
    // Alerts only have one outcome (OK) — just report it and move on.
    emit dialogRequested("alert", msg);
    emit dialogResolved("alert");
}

bool PiggyPage::javaScriptConfirm(const QUrl &securityOrigin, const QString &msg) {
    Q_UNUSED(securityOrigin);
    if (m_autoAction == "manual") {
        return waitForManualResolution("confirm", msg, nullptr);
    }
    emit dialogRequested("confirm", msg);
    bool accept = (m_autoAction != "dismiss"); // default: accept
    emit dialogResolved("confirm");
    return accept;
}

bool PiggyPage::javaScriptPrompt(const QUrl &securityOrigin, const QString &msg,
                                  const QString &defaultValue, QString *result) {
    Q_UNUSED(securityOrigin);
    if (m_autoAction == "manual") {
        QString text;
        bool accept = waitForManualResolution("prompt", msg, &text);
        if (result) *result = text;
        return accept;
    }
    emit dialogRequested("prompt", msg);
    bool accept = (m_autoAction != "dismiss");
    if (result) *result = defaultValue;
    emit dialogResolved("prompt");
    return accept;
}

// ─── File-picker override (used by "upload") ──────────────────────────────────

QStringList PiggyPage::chooseFiles(QWebEnginePage::FileSelectionMode mode,
                                    const QStringList &oldFiles,
                                    const QStringList &acceptedMimeTypes) {
    Q_UNUSED(mode);
    Q_UNUSED(oldFiles);
    Q_UNUSED(acceptedMimeTypes);

    if (!m_pendingUploadPath.isEmpty()) {
        QStringList result = { m_pendingUploadPath };
        m_pendingUploadPath.clear(); // one-shot — next native picker gets nothing unless set again
        return result;
    }
    return QStringList(); // no upload staged — behaves like the user cancelled the picker
}