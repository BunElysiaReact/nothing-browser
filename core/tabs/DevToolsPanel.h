#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSplitter>
#include <QComboBox>
#include <QTimer>
#include <QMap>
#include <QDir>
#include <QRegularExpression>
#include "NetworkCapture.h"

// ── Welcome / T&C splash ──────────────────────────────────────────────────────
class WelcomeScreen : public QWidget {
    Q_OBJECT
public:
    explicit WelcomeScreen(QWidget *parent = nullptr);
signals:
    void accepted();
};

// ── Main DevTools panel ───────────────────────────────────────────────────────
class DevToolsPanel : public QWidget {
    Q_OBJECT
public:
    explicit DevToolsPanel(QWidget *parent = nullptr);

public slots:
    void onRequestCaptured(const CapturedRequest &req);
    void onWsFrame(const WebSocketFrame &frame);
    void onCookieCaptured(const CapturedCookie &cookie);
    void onCookieRemoved(const QString &name, const QString &domain);
    void onStorageCaptured(const QString &origin, const QString &key,
                           const QString &value, const QString &storageType);
    void onRawRequest(const QString &method, const QString &url, const QString &headers);

    void setCurrentUrl(const QString &url) { m_currentUrl = url; }
    bool exportSession(const QString &path);
    bool importSession(const QString &path);
    QString lastSessionPath() const { return m_lastSessionPath; }

private slots:
    void filterNetwork(const QString &text);
    void clearAll();
    void onNetworkRowSelected(int row, int col);
    void onWsRowSelected(int row, int col);
    void onCookieRowSelected(int row, int col);
    void onStorageRowSelected(int row, int col);
    void exportSelected();
    void downloadSelected();

private:
    QTabWidget *m_tabs;

    // Network
    QWidget      *buildNetworkTab();
    QTableWidget *m_netTable;
    QTabWidget   *m_netDetailTabs;
    QTextEdit    *m_netHeadersView;
    QTextEdit    *m_netResponseView;
    QTextEdit    *m_netRawView;
    QLabel       *m_netCount;
    QLineEdit    *m_netFilter;
    QComboBox    *m_typeFilter;
    int           m_netTotal = 0;

    // WebSocket
    QWidget      *buildWsTab();
    QTableWidget *m_wsTable;
    QTextEdit    *m_wsDetail;
    QLabel       *m_wsCount;
    int           m_wsTotal = 0;

    // Cookies
    QWidget      *buildCookiesTab();
    QTableWidget *m_cookieTable;
    QTabWidget   *m_cookieDetailTabs;
    QTextEdit    *m_cookieAttrView;
    QTextEdit    *m_cookieRequestView;
    QLabel       *m_cookieCount;
    int           m_cookieTotal = 0;
    QMap<QString, int> m_cookieRowMap;

    // Storage
    QWidget      *buildStorageTab();
    QTableWidget *m_storageTable;
    QTextEdit    *m_storageDetail;
    QLabel       *m_storageCount;
    int           m_storageTotal = 0;

    // Export
    QWidget   *buildExportTab();
    QTextEdit *m_exportArea;
    QComboBox *m_exportFormat;

    // Data
    struct NetEntry {
        QString method, url, status, type, mime, body, reqHeaders, resHeaders;
    };
    struct CookieEntry {
        CapturedCookie cookie;
        QString setByUrl, setByMethod, setByHeaders;
    };

    QList<NetEntry>       m_netEntries;
    QList<WebSocketFrame> m_wsFrames;
    QList<CookieEntry>    m_cookieEntries;
    QString               m_lastUrl, m_lastMethod, m_lastHeaders;
    QString               m_lastSessionPath;
    QString               m_currentUrl;

    // Helpers
    void    updateTabLabel(int idx, const QString &name, int count);
    QString buildHeadersBlock(const NetEntry &e) const;
    QString buildSummaryBlock(const NetEntry &e) const;
    QString buildRaw(const NetEntry &e) const;
    QString generatePython(const QString &method, const QString &url, const QString &headers);
    QString generateCurl(const QString &method, const QString &url, const QString &headers);
    QString generateJS(const QString &method, const QString &url);

    static void clip(const QString &text);
    static QPushButton *btn(const QString &label, const QString &color, QWidget *parent);
    static QString panelStyle();
    static QTableWidgetItem *makeItem(const QString &text,
                                      const QColor  &color = QColor("#cccccc"));
};