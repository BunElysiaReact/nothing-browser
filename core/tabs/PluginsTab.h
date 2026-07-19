#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include "../engine/PluginManager.h"

class PluginsTab : public QWidget {
    Q_OBJECT
public:
    explicit PluginsTab(QWidget *parent = nullptr);

private slots:
    void onInstalledSelected(int row);
    void onCommunitySelected(int row);
    void onTogglePlugin();
    void onUninstall();
    void onInstallFromRepo();
    void onRefreshCommunity();
    void onCommunityListReady(const QList<PluginManifest> &list);
    void onInstallComplete(const QString &id, bool ok, const QString &err);

private:
    void buildUI();
    void refreshInstalledList();
    void refreshCommunityList();
    QString panelStyle();

    QTabWidget   *m_tabs;

    // Installed tab
    QListWidget  *m_installedList;
    QLabel       *m_instName;
    QLabel       *m_instAuthor;
    QLabel       *m_instVersion;
    QTextEdit    *m_instDesc;
    QTextEdit    *m_instHowTo;
    QLabel       *m_instRestart;
    QLabel       *m_instPerms;
    QPushButton  *m_toggleBtn;
    QPushButton  *m_uninstallBtn;

    // Community tab
    QListWidget  *m_communityList;
    QLabel       *m_comName;
    QLabel       *m_comAuthor;
    QLabel       *m_comVersion;
    QTextEdit    *m_comDesc;
    QTextEdit    *m_comHowTo;
    QLabel       *m_comRestart;
    QLabel       *m_comPerms;
    QPushButton  *m_installBtn;
    QPushButton  *m_refreshBtn;
    QLabel       *m_comStatus;

    QList<PluginManifest> m_communityCache;
};