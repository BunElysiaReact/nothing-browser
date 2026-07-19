#pragma once

#include <QWidget>
#include <QObject>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineUrlRequestInterceptor>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QTreeWidget>
#include <QTextEdit>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>

// --- Forward Declarations ---
class PiggyServer;
class NetworkCapture;
class Interceptor;
struct CapturedRequest;
struct WebSocketFrame;
struct CapturedCookie;

// --- PiggyTab Class ---

class PiggyTab : public QWidget {
    Q_OBJECT
public:
    explicit PiggyTab(QWidget *parent = nullptr);

    static QString domExtractorJS();
    static QString highlightJS(const QString &selector);
    static QString clearHighlightJS();
    static QString clickListenerJS();
    static QString hoverTooltipJS();
    static QString removeHoverTooltipJS();

    QWebEnginePage* getPage() const { return m_page; }

private slots:
    void onNavigate();
    void onPageLoaded(bool ok);
    void toggleInspectMode(bool enabled);
    void onSelectorChanged(const QString &text);
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onExportClicked();
    void pollDomChanges();

private:
    void setupUI();
    void setupEngine();
    void injectDomExtractor();
    void injectClickListener();
    void injectHoverTooltip();
    void removeHoverTooltip();
    void buildTree(const QJsonArray &nodes, QTreeWidgetItem *parent);
    void onPageElementClicked(const QString &selector, const QString &tag,
                              const QString &id, const QString &cls, int w, int h);
    void selectTreeItemBySelector(const QString &selector);
    void updateInfoHUD(const QString &tag, const QString &cls,
                       const QString &sel, int w, int h);

    QLineEdit      *m_urlBar      = nullptr;
    QPushButton    *m_goBtn       = nullptr;
    QPushButton    *m_inspectBtn  = nullptr;
    QPushButton    *m_exportBtn   = nullptr;
    QLabel         *m_statusLabel = nullptr;
    QSplitter      *m_splitter    = nullptr;
    QTreeWidget    *m_tree        = nullptr;
    QLineEdit      *m_selectorBar = nullptr;
    QTextEdit      *m_infoPanel   = nullptr;
    QWebEngineView *m_mirror      = nullptr;

    QWebEnginePage    *m_page        = nullptr;
    QWebEngineProfile *m_profile     = nullptr;
    QTimer            *m_domPollTimer = nullptr;

    bool m_inspectModeActive = false;
};