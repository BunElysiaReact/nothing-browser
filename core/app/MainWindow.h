#pragma once
#include <QMainWindow>
#include <QTabWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include <QCloseEvent>
#include <QProcess>
#include "../tabs/DevToolsPanel.h"
#include "../tabs/NewsTab.h"
#include "../engine/UpdateChecker.h"
#include "../tabs/PluginsTab.h"

class BrowserTab;
class YoutubeTab;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void saveSession();
    void loadSession();
    void quickSaveSession(const QString &name);

private:
    QStackedWidget *m_stack;
    WelcomeScreen  *m_welcome;
    QWidget        *m_main;

    QTabWidget     *m_tabs;
    DevToolsPanel  *m_devtools;
    BrowserTab     *m_browser;
    YoutubeTab     *m_youtube;
    NewsTab        *m_news;
    PluginsTab *m_plugins;

    UpdateChecker  *m_checker;

    void    setupTabs();
    void    setupStyle();
    void    setupSessionMenu();
    QString sessionsDir() const;
    QIcon   tabIcon(const QString &color);
};