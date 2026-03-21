#include "UpdateChecker.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>

// ═════════════════════════════════════════════════════════════════════════════
//  Construction
// ═════════════════════════════════════════════════════════════════════════════
UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {
    m_nam   = new QNetworkAccessManager(this);
    m_timer = new QTimer(this);

    connect(m_timer, &QTimer::timeout, this, &UpdateChecker::checkNow);

    // Check after 3s on startup, then every 6 hours
    QTimer::singleShot(3000, this, &UpdateChecker::checkNow);
    m_timer->start(6 * 60 * 60 * 1000);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Check
// ═════════════════════════════════════════════════════════════════════════════
void UpdateChecker::checkNow() {
    QNetworkRequest req{QUrl(GITHUB_API_URL)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QString("NothingBrowser/%1").arg(CURRENT_VERSION));
    // GitHub API requires Accept header
    req.setRawHeader("Accept", "application/vnd.github+json");

    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onCheckReply(reply);
    });
}

void UpdateChecker::onCheckReply(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit checkFailed(reply->errorString());
        return;
    }

    // GitHub may redirect — follow handled by QNAM automatically
    // Check for rate limit
    QByteArray data = reply->readAll();
    if (data.contains("\"rate limit\"") || data.contains("API rate limit")) {
        emit checkFailed("GitHub API rate limit hit — try again in an hour");
        return;
    }

    VersionInfo info = parseGithubRelease(data);
    if (info.version.isEmpty()) {
        emit checkFailed("Could not parse release info from GitHub");
        return;
    }

    info.isNewer = versionToInt(info.version) > versionToInt(CURRENT_VERSION);

    if (info.isNewer) emit updateAvailable(info);
    else              emit noUpdate(info);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Parse GitHub Releases API response
// ═════════════════════════════════════════════════════════════════════════════
VersionInfo UpdateChecker::parseGithubRelease(const QByteArray &data) {
    VersionInfo info;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return info;

    QJsonObject obj = doc.object();

    // tag_name is like "v0.1.2" or "0.1.2"
    QString tag = obj["tag_name"].toString();
    if (tag.startsWith('v') || tag.startsWith('V'))
        tag = tag.mid(1);
    info.version     = tag;
    info.releaseName = obj["name"].toString();
    info.releaseNotes = obj["body"].toString();

    // Find the Linux binary asset — look for .tar.gz or AppImage
    // Priority: .tar.gz > AppImage > anything
    QJsonArray assets = obj["assets"].toArray();
    for (const auto &v : assets) {
        QJsonObject asset = v.toObject();
        QString name = asset["name"].toString().toLower();
        QString url  = asset["browser_download_url"].toString();

        // Prefer linux tar.gz
        if (name.contains("linux") && name.endsWith(".tar.gz")) {
            info.downloadUrl = url;
            break;
        }
        // Fallback: any tar.gz
        if (name.endsWith(".tar.gz") && info.downloadUrl.isEmpty())
            info.downloadUrl = url;

        // Fallback: AppImage
        if (name.endsWith(".appimage") && info.downloadUrl.isEmpty())
            info.downloadUrl = url;
    }

    // If no asset found, fall back to the zipball
    if (info.downloadUrl.isEmpty())
        info.downloadUrl = obj["zipball_url"].toString();

    // Parse changelog from release body (markdown)
    info.changelog = parseChangelog(info.releaseNotes);

    return info;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Parse markdown release body into changelog entries
//  Looks for lines like:  - Added: something
//                          - Fixed: something
//                          - feat: something
// ═════════════════════════════════════════════════════════════════════════════
QList<ChangeEntry> UpdateChecker::parseChangelog(const QString &body) {
    QList<ChangeEntry> entries;
    if (body.isEmpty()) return entries;

    const QStringList lines = body.split('\n');
    for (const QString &raw : lines) {
        QString line = raw.trimmed();
        if (line.isEmpty()) continue;

        // Strip leading list markers
        if (line.startsWith("- ") || line.startsWith("* "))
            line = line.mid(2).trimmed();
        else if (line.startsWith("• "))
            line = line.mid(2).trimmed();
        else
            continue; // only parse list items

        QString type = "change";
        QString text = line;

        // Detect type from prefix keywords
        QString lower = line.toLower();
        if (lower.startsWith("add") || lower.startsWith("feat") || lower.startsWith("new"))
            type = "added";
        else if (lower.startsWith("fix") || lower.startsWith("bug") || lower.startsWith("patch"))
            type = "fix";
        else if (lower.startsWith("coming") || lower.startsWith("planned") || lower.startsWith("wip"))
            type = "coming";

        // Strip prefix like "Added: " or "Fix: "
        int colon = text.indexOf(':');
        if (colon > 0 && colon < 12)
            text = text.mid(colon + 1).trimmed();

        if (!text.isEmpty())
            entries.append({type, text});
    }

    // If nothing parsed from markdown, add a single entry with the version
    if (entries.isEmpty() && !body.isEmpty())
        entries.append({"added", body.left(120).replace('\n', ' ')});

    return entries;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Download
// ═════════════════════════════════════════════════════════════════════════════
QString UpdateChecker::downloadDir() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                  + "/nothing-browser-update";
    QDir().mkpath(dir);
    return dir;
}

void UpdateChecker::startDownload(const VersionInfo &info) {
    if (info.downloadUrl.isEmpty()) {
        emit downloadFailed("No download URL available for this release");
        return;
    }

    m_pending = info;

    // Determine filename from URL
    QString url      = info.downloadUrl;
    QString filename = url.section('/', -1);
    if (filename.isEmpty()) filename = "nothing-browser-update.tar.gz";

    QString outPath = downloadDir() + "/" + filename;

    // Remove stale download
    QFile::remove(outPath);

    m_dlFile = new QFile(outPath, this);
    if (!m_dlFile->open(QIODevice::WriteOnly)) {
        emit downloadFailed("Cannot write to: " + outPath);
        delete m_dlFile; m_dlFile = nullptr;
        return;
    }

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QString("NothingBrowser/%1").arg(CURRENT_VERSION));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    m_dlReply = m_nam->get(req);

    connect(m_dlReply, &QNetworkReply::downloadProgress,
            this, &UpdateChecker::onDownloadProgress);
    connect(m_dlReply, &QNetworkReply::finished,
            this, &UpdateChecker::onDownloadFinished);
    connect(m_dlReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_dlFile) m_dlFile->write(m_dlReply->readAll());
    });
}

void UpdateChecker::onDownloadProgress(qint64 received, qint64 total) {
    if (total > 0)
        emit downloadProgress((int)(received * 100 / total));
}

void UpdateChecker::onDownloadFinished() {
    if (!m_dlReply || !m_dlFile) return;

    m_dlReply->deleteLater();

    if (m_dlReply->error() != QNetworkReply::NoError) {
        QString errStr = m_dlReply->errorString();
        m_dlFile->close();
        QFile::remove(m_dlFile->fileName());
        delete m_dlFile; m_dlFile = nullptr;
        m_dlReply = nullptr;
        emit downloadFailed(errStr);
        return;
    }

    // Flush remaining bytes
    m_dlFile->write(m_dlReply->readAll());
    m_dlFile->flush();
    m_dlFile->close();

    QString archivePath = m_dlFile->fileName();
    delete m_dlFile; m_dlFile = nullptr;
    m_dlReply = nullptr;

    emit downloadProgress(100);
    emit downloadReady(archivePath, m_pending);
}

void UpdateChecker::cancelDownload() {
    if (m_dlReply) {
        m_dlReply->abort();
        m_dlReply->deleteLater();
        m_dlReply = nullptr;
    }
    if (m_dlFile) {
        m_dlFile->close();
        QFile::remove(m_dlFile->fileName());
        delete m_dlFile;
        m_dlFile = nullptr;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Install — write updater.sh, execute it, exit app
//
//  The script:
//  1. Waits for the app to exit (sleep 1)
//  2. Extracts the archive into a temp dir
//  3. Finds the binary inside (named "nothing-browser")
//  4. Replaces the current executable
//  5. Restarts the app
//  6. Cleans up
// ═════════════════════════════════════════════════════════════════════════════
void UpdateChecker::installUpdate(const QString &archivePath) {
    QString appPath    = QCoreApplication::applicationFilePath();
    QString appDir     = QFileInfo(appPath).absolutePath();
    QString appName    = QFileInfo(appPath).fileName();
    QString scriptPath = downloadDir() + "/updater.sh";
    QString extractDir = downloadDir() + "/extracted";

    // Does the binary live somewhere we need elevated perms to write?
    bool needsSudo = !QFileInfo(appPath).isWritable();

    // Extract command
    QString extractCmd;
    if (archivePath.endsWith(".tar.gz") || archivePath.endsWith(".tgz"))
        extractCmd = QString("tar -xzf \"%1\" -C \"%2\"").arg(archivePath, extractDir);
    else if (archivePath.endsWith(".zip"))
        extractCmd = QString("unzip -o \"%1\" -d \"%2\"").arg(archivePath, extractDir);
    else if (archivePath.endsWith(".AppImage") || archivePath.endsWith(".appimage"))
        extractCmd = QString("cp \"%1\" \"%2/%3\"").arg(archivePath, extractDir, appName);
    else
        extractCmd = QString("tar -xf \"%1\" -C \"%2\"").arg(archivePath, extractDir);

    // Build the updater script
    // Uses pkexec for a GUI password prompt when the binary needs root
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit downloadFailed("Cannot write updater script to: " + scriptPath);
        return;
    }

    QTextStream s(&scriptFile);
    s << "#!/bin/bash\n";
    s << "# Nothing Browser auto-updater\n";
    s << "ORIGINAL_USER=\"" << qgetenv("USER") << "\"\n";
    s << "APP_PATH=\""    << appPath    << "\"\n";
    s << "APP_DIR=\""     << appDir     << "\"\n";
    s << "APP_NAME=\""    << appName    << "\"\n";
    s << "ARCHIVE=\""     << archivePath << "\"\n";
    s << "EXTRACT_DIR=\"" << extractDir << "\"\n\n";

    s << "sleep 1\n\n";

    s << "mkdir -p \"$EXTRACT_DIR\"\n";
    s << extractCmd << "\n\n";

    s << "NEW_BIN=$(find \"$EXTRACT_DIR\" -type f -name \"$APP_NAME\" | head -1)\n";
    s << "if [ -z \"$NEW_BIN\" ]; then\n";
    s << "    NEW_BIN=$(find \"$EXTRACT_DIR\" -maxdepth 4 -type f -executable \\\n";
    s << "              ! -name '*.so*' ! -name '*.jar' ! -name '*.sh' | head -1)\n";
    s << "fi\n";
    s << "if [ -z \"$NEW_BIN\" ]; then\n";
    s << "    echo 'ERROR: binary not found in archive'\n";
    s << "    exit 1\n";
    s << "fi\n\n";

    // Copy with or without sudo
    if (needsSudo) {
        s << "# Need elevated permissions — try pkexec, fall back to sudo\n";
        s << "cp \"$APP_PATH\" \"$APP_PATH.bak\" 2>/dev/null || true\n";
        s << "pkexec cp \"$NEW_BIN\" \"$APP_PATH\" 2>/dev/null \\\n";
        s << "    || sudo cp \"$NEW_BIN\" \"$APP_PATH\"\n";
        s << "pkexec chmod +x \"$APP_PATH\" 2>/dev/null \\\n";
        s << "    || sudo chmod +x \"$APP_PATH\"\n\n";
    } else {
        s << "cp \"$APP_PATH\" \"$APP_PATH.bak\" 2>/dev/null || true\n";
        s << "cp \"$NEW_BIN\" \"$APP_PATH\"\n";
        s << "chmod +x \"$APP_PATH\"\n\n";
    }

    s << "# Clean up\n";
    s << "rm -rf \"$EXTRACT_DIR\"\n";
    s << "rm -f \"$ARCHIVE\"\n\n";

    s << "# Restart as the original user\n";
    s << "if [ \"$(id -u)\" = \"0\" ] && [ -n \"$ORIGINAL_USER\" ]; then\n";
    s << "    su - \"$ORIGINAL_USER\" -c \"\\\"$APP_PATH\\\" &\"\n";
    s << "else\n";
    s << "    \"$APP_PATH\" &\n";
    s << "fi\n\n";

    s << "rm -f \"$0\"\n";
    scriptFile.close();

    // Make executable
    QFile::setPermissions(scriptPath,
        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
        QFile::ReadGroup | QFile::ExeGroup);

    emit installStarted();

    // Launch detached — app exits so script can replace the binary
    QProcess::startDetached("/bin/bash", {scriptPath});
    QCoreApplication::quit();
}


// ═════════════════════════════════════════════════════════════════════════════
//  Helpers
// ═════════════════════════════════════════════════════════════════════════════
int UpdateChecker::versionToInt(const QString &v) {
    // "1.2.3" → 10203
    auto parts = v.split('.');
    while (parts.size() < 3) parts << "0";
    return parts[0].toInt() * 10000
         + parts[1].toInt() * 100
         + parts[2].toInt();
}