#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWebEngineProfile>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

struct PluginManifest {
    QString id;
    QString name;
    QString version;
    QString description;
    QString author;
    QString howToUse;
    bool    requiresRestart = false;
    bool    enabled         = false;
    bool    installed       = false;
    QString installPath;
    QStringList permissions;
};

class PluginManager : public QObject {
    Q_OBJECT
public:
    static PluginManager &instance() {
        static PluginManager p;
        return p;
    }

    // Load all installed+enabled plugins into a profile
    void injectAll(QWebEngineProfile *profile);

    // Install from local folder path
    bool installPlugin(const QString &folderPath);

    // Uninstall by id
    bool uninstallPlugin(const QString &id);

    // Enable/disable without uninstall
    void setEnabled(const QString &id, bool enabled);

    // Fetch community plugin list from GitHub
    void fetchCommunityList();

    // Clone/download a specific plugin from repo
    void installFromRepo(const QString &pluginId);

    QString pluginsDir() const;
    QList<PluginManifest> installedPlugins() const { return m_installed; }
    QList<PluginManifest> communityPlugins() const { return m_community; }

signals:
    void communityListReady(const QList<PluginManifest> &plugins);
    void installComplete(const QString &id, bool success, const QString &error);
    void pluginListChanged();

private:
    PluginManager();
    void scanInstalled();
    PluginManifest parseManifest(const QString &path);
    void saveState();
    void loadState();

    QList<PluginManifest>  m_installed;
    QList<PluginManifest>  m_community;
    QNetworkAccessManager *m_nam = nullptr;

    static constexpr const char *REPO_API =
        "https://api.github.com/repos/ernest-tech-house-co-operation/"
        "nothing-browser-plugins/contents/plugins";
    static constexpr const char *REPO_RAW =
        "https://raw.githubusercontent.com/ernest-tech-house-co-operation/"
        "nothing-browser-plugins/main/plugins";
};