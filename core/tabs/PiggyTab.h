#pragma once
#include <QWidget>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSplitter>
#include <QLabel>
#include <QTextEdit>
#include <QTimer>
#include <QWebChannel>

class PiggyTab : public QWidget {
    Q_OBJECT
public:
    explicit PiggyTab(QWidget *parent = nullptr);

public:
    QWebEnginePage* getPage() const { return m_page; }
    static QString domExtractorJS();

private slots:
    void onNavigate();
    void onPageLoaded(bool ok);
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onSelectorChanged(const QString &text);
    void onExportClicked();
    void pollDomChanges();
    void toggleInspectMode(bool enabled);

    // Called from JS when the user clicks an element on the live page
    void onPageElementClicked(const QString &selector, const QString &tag,
                              const QString &id,  const QString &cls,
                              int w, int h);

private:
    // ── UI ────────────────────────────────────────────────────────────────────
    QLineEdit    *m_urlBar;
    QPushButton  *m_goBtn;
    QPushButton  *m_inspectBtn;   // NEW – toggles inspect/hover mode
    QLabel       *m_statusLabel;
    QSplitter    *m_splitter;

    // Left panel
    QLineEdit    *m_selectorBar;
    QTreeWidget  *m_tree;
    QPushButton  *m_exportBtn;

    // Right panel – live mirror
    QWebEngineView *m_mirror;

    // Info panel – bottom
    QTextEdit    *m_infoPanel;

    // ── Engine ────────────────────────────────────────────────────────────────
    QWebEngineProfile *m_profile;
    QWebEnginePage    *m_page;
    QTimer            *m_domPollTimer;

    bool m_inspectModeActive = false;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void setupUI();
    void setupEngine();
    void buildTree(const QJsonArray &nodes, QTreeWidgetItem *parent);
    void injectDomExtractor();
    void injectClickListener();   // NEW
    void injectHoverTooltip();    // NEW
    void removeHoverTooltip();    // NEW
    void selectTreeItemBySelector(const QString &selector);  // NEW
    void updateInfoHUD(const QString &tag, const QString &cls,
                       const QString &sel, int w, int h);   // NEW

    
    static QString highlightJS(const QString &selector);
    static QString clearHighlightJS();
    static QString clickListenerJS();   // NEW
    static QString hoverTooltipJS();    // NEW
    static QString removeHoverTooltipJS(); // NEW
};