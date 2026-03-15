#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include "../engine/UpdateChecker.h"

// ─────────────────────────────────────────────────────────────────────────────
//  NotificationBell  —  lives in the top bar, lights up on update
// ─────────────────────────────────────────────────────────────────────────────
class NotificationBell : public QPushButton {
    Q_OBJECT
public:
    explicit NotificationBell(QWidget *parent = nullptr);
    void setUnread(int count);
    void clearUnread();
private:
    int     m_unread = 0;
    QLabel *m_badge;
    void    refreshStyle();
};

// ─────────────────────────────────────────────────────────────────────────────
//  NewsTab  —  home hub: quick scrapper panel + updates + changelog
// ─────────────────────────────────────────────────────────────────────────────
class NewsTab : public QWidget {
    Q_OBJECT
public:
    explicit NewsTab(QWidget *parent = nullptr);

    // MainWindow calls this to hand off the shared UpdateChecker
    void attachChecker(UpdateChecker *checker);

    // Returns the bell widget so MainWindow can embed it in the tab bar area
    NotificationBell *bell() const { return m_bell; }

signals:
    // Emitted when user clicks "Open Scrapper" quick-access
    void openScrapper();
    void openBrowser();

private slots:
    void onUpdateAvailable(const VersionInfo &info);
    void onNoUpdate(const VersionInfo &info);
    void onCheckFailed(const QString &err);
    void onManualCheck();

private:
    UpdateChecker    *m_checker = nullptr;
    NotificationBell *m_bell;

    // UI sections
    QLabel    *m_versionLabel;
    QLabel    *m_updateStatus;
    QPushButton *m_checkBtn;
    QPushButton *m_downloadBtn;
    QWidget   *m_changelogWidget;
    QVBoxLayout *m_changelogLayout;

    void buildUI();
    void renderChangelog(const QList<ChangeEntry> &entries);
    void setUpdateBanner(bool hasUpdate, const QString &version, const QString &url);

    static QString s();   // shared stylesheet
    static QPushButton *actionBtn(const QString &label, const QString &color,
                                   QWidget *parent);
    static QFrame *separator(QWidget *parent);
};