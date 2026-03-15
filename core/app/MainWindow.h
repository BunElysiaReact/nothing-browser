#pragma once
#include <QMainWindow>
#include <QTabWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include "../tabs/DevToolsPanel.h"
#include "../tabs/NewsTab.h"
#include "../engine/UpdateChecker.h"

class BrowserTab;
class YoutubeTab;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    QStackedWidget *m_stack;
    WelcomeScreen  *m_welcome;
    QWidget        *m_main;

    QTabWidget     *m_tabs;
    DevToolsPanel  *m_devtools;
    BrowserTab     *m_browser;
    YoutubeTab     *m_youtube;
    NewsTab        *m_news;

    UpdateChecker  *m_checker;

    void setupTabs();
    void setupStyle();
    QIcon tabIcon(const QString &color);
};