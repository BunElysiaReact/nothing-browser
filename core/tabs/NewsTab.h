#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include "UpdateChecker.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Notification bell with unread badge
// ─────────────────────────────────────────────────────────────────────────────
class NotificationBell : public QPushButton {
    Q_OBJECT
public:
    explicit NotificationBell(QWidget *parent = nullptr);
    void setUnread(int count);
    void clearUnread();
    int  unread() const { return m_unread; }
private:
    void    refreshStyle();
    QLabel *m_badge  = nullptr;
    int     m_unread = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  NewsTab
// ─────────────────────────────────────────────────────────────────────────────
class NewsTab : public QWidget {
    Q_OBJECT
public:
    explicit NewsTab(QWidget *parent = nullptr);

    void attachChecker(UpdateChecker *checker);
    NotificationBell *bell() const { return m_bell; }
    void renderChangelog(const QList<ChangeEntry> &entries);

signals:
    void openBrowser();
    void openScrapper();

private slots:
    void onUpdateAvailable(const VersionInfo &info);
    void onNoUpdate(const VersionInfo &info);
    void onCheckFailed(const QString &error);
    void onManualCheck();
    void onDownloadReady(const QString &path, const VersionInfo &info);
    void onDownloadFailed(const QString &error);

private:
    void buildUI();
    void setDownloadProgress(int pct);
    void setUpdateBanner(bool hasUpdate, const QString &version, const QString &url);

    // ── Top bar ───────────────────────────────────────────────────────────────
    QLabel           *m_versionLabel  = nullptr;
    NotificationBell *m_bell          = nullptr;

    // ── Update card ───────────────────────────────────────────────────────────
    QLabel      *m_updateStatus   = nullptr;
    QPushButton *m_checkBtn       = nullptr;
    QPushButton *m_downloadBtn    = nullptr;
    QPushButton *m_installBtn     = nullptr;
    QPushButton *m_cancelBtn      = nullptr;

    // Progress bar
    QWidget *m_progressWrap  = nullptr;
    QWidget *m_progressFill  = nullptr;
    QLabel  *m_progressLabel = nullptr;

    // ── Changelog card ────────────────────────────────────────────────────────
    QWidget     *m_changelogWidget = nullptr;
    QVBoxLayout *m_changelogLayout = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    UpdateChecker *m_checker        = nullptr;
    VersionInfo    m_pendingInfo;
    QString        m_downloadedPath;

    // ── Helpers ───────────────────────────────────────────────────────────────
    static QString    s();
    static QPushButton *actionBtn(const QString &label, const QString &color,
                                   QWidget *parent);
    static QFrame     *separator(QWidget *parent);
};