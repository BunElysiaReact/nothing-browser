#include "PluginsTab.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QPixmap>

QString PluginsTab::panelStyle() {
    return R"(
        QWidget   { background:#0d0d0d; color:#cccccc; }
        QTabWidget::pane { border:none; background:#0d0d0d; }
        QTabBar::tab {
            background:#111; color:#555; padding:6px 16px;
            border:none; font-family:monospace; font-size:11px;
        }
        QTabBar::tab:selected { background:#0d0d0d; color:#00cc66; border-bottom:2px solid #00cc66; }
        QTabBar::tab:hover { color:#aaa; }
        QListWidget {
            background:#080808; border:none;
            font-family:monospace; font-size:11px; color:#888;
        }
        QListWidget::item { padding:10px 14px; border-bottom:1px solid #141414; }
        QListWidget::item:selected { background:#0a2a0a; color:#00cc66; border-left:3px solid #00cc66; }
        QListWidget::item:hover:!selected { background:#111; }
        QTextEdit {
            background:#080808; color:#888; border:none;
            font-family:monospace; font-size:11px; padding:8px;
        }
        QLabel { background:transparent; }
        QScrollBar:vertical { background:#090909; width:4px; border:none; }
        QScrollBar::handle:vertical { background:#1e1e1e; border-radius:2px; min-height:16px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
    )";
}

static QPushButton *pBtn(const QString &label, const QString &color, QWidget *parent) {
    auto *b = new QPushButton(label, parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QString(R"(
        QPushButton {
            background:#0d0d0d; color:%1; border:1px solid %1;
            border-radius:2px; font-family:monospace; font-size:10px;
            padding:5px 14px;
        }
        QPushButton:hover { background:%2; }
        QPushButton:disabled { color:#333; border-color:#222; }
    )").arg(color)
       .arg(color=="#00cc66"?"#001800":color=="#ff4444"?"#1a0000":
            color=="#0088ff"?"#001020":"#161616"));
    return b;
}

static QLabel *infoLabel(const QString &text, QWidget *p) {
    auto *l = new QLabel(text, p);
    l->setStyleSheet("color:#555; font-family:monospace; font-size:10px;");
    return l;
}

static QLabel *valueLabel(const QString &text, const QString &color, QWidget *p) {
    auto *l = new QLabel(text, p);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-family:monospace; font-size:11px;").arg(color));
    return l;
}

PluginsTab::PluginsTab(QWidget *parent) : QWidget(parent) {
    setStyleSheet(panelStyle());
    buildUI();
    refreshInstalledList();

    connect(&PluginManager::instance(), &PluginManager::communityListReady,
            this, &PluginsTab::onCommunityListReady);
    connect(&PluginManager::instance(), &PluginManager::installComplete,
            this, &PluginsTab::onInstallComplete);
    connect(&PluginManager::instance(), &PluginManager::pluginListChanged,
            this, [this]() { refreshInstalledList(); });
}

void PluginsTab::buildUI() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // ── Header bar ────────────────────────────────────────────────────────────
    auto *hdr = new QWidget(this);
    hdr->setFixedHeight(40);
    hdr->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(16,0,16,0);
    auto *title = new QLabel("⬡  PLUGINS", hdr);
    title->setStyleSheet("color:#00cc66; font-family:monospace; font-size:13px; font-weight:bold; letter-spacing:2px;");
    auto *sub = new QLabel("community extensions for nothing browser", hdr);
    sub->setStyleSheet("color:#333; font-family:monospace; font-size:10px;");
    hl->addWidget(title);
    hl->addSpacing(16);
    hl->addWidget(sub);
    hl->addStretch();
    root->addWidget(hdr);

    m_tabs = new QTabWidget(this);

    // ════════════════════════════════════════════════════
    //  INSTALLED tab
    // ════════════════════════════════════════════════════
    auto *instWidget = new QWidget;
    instWidget->setStyleSheet(panelStyle());

    // ── Elysia background image ───────────────────────────────────────────────
    // Placed directly on instWidget, behind everything, centered, low opacity
    auto *bgLabel = new QLabel(instWidget);
    QPixmap elysiaPix(":/icons/elysialogo.svg");
    if (elysiaPix.isNull())
        elysiaPix = QPixmap(":/icons/elysialogo.svg");
    if (!elysiaPix.isNull()) {
        bgLabel->setPixmap(elysiaPix.scaled(420, 420, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        bgLabel->setGeometry(
            (instWidget->width()  - 420) / 2,
            (instWidget->height() - 420) / 2,
            420, 420
        );
        bgLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        bgLabel->setStyleSheet("opacity: 0.04; background: transparent;");
        bgLabel->lower();

        // Re-center when instWidget resizes
        connect(instWidget, &QWidget::customContextMenuRequested, instWidget, [bgLabel, instWidget](){
            bgLabel->move((instWidget->width()-420)/2, (instWidget->height()-420)/2);
        });
    }

    auto *il = new QHBoxLayout(instWidget);
    il->setContentsMargins(0,0,0,0);
    il->setSpacing(0);

    // Left: list
    auto *listWrap = new QWidget;
    listWrap->setFixedWidth(260);
    listWrap->setStyleSheet("border-right:1px solid #1a1a1a;");
    auto *lw = new QVBoxLayout(listWrap);
    lw->setContentsMargins(0,0,0,0);
    lw->setSpacing(0);

    auto *instHdr = new QWidget;
    instHdr->setFixedHeight(30);
    instHdr->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *ihl = new QHBoxLayout(instHdr);
    ihl->setContentsMargins(10,0,10,0);
    ihl->addWidget(infoLabel("INSTALLED", instHdr));
    ihl->addStretch();
    lw->addWidget(instHdr);

    m_installedList = new QListWidget(listWrap);
    lw->addWidget(m_installedList, 1);

    auto *importBtn = pBtn("+ FROM FOLDER", "#888888", listWrap);
    importBtn->setStyleSheet(importBtn->styleSheet() + "border-radius:0; border-top:1px solid #1a1a1a;");
    connect(importBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(this, "Select Plugin Folder");
        if (path.isEmpty()) return;
        bool ok = PluginManager::instance().installPlugin(path);
        if (!ok) QMessageBox::warning(this, "Install Failed", "No valid manifest.json found in that folder.");
    });
    lw->addWidget(importBtn);
    il->addWidget(listWrap);

    // Right: detail panel
    auto *detailWrap = new QWidget;
    detailWrap->setStyleSheet("background:transparent;");
    auto *dv = new QVBoxLayout(detailWrap);
    dv->setContentsMargins(20,20,20,20);
    dv->setSpacing(12);

    m_instName    = valueLabel("No plugin selected", "#00cc66", detailWrap);
    m_instName->setStyleSheet("color:#00cc66; font-family:monospace; font-size:14px; font-weight:bold;");
    m_instAuthor  = valueLabel("—", "#555", detailWrap);
    m_instVersion = valueLabel("—", "#333", detailWrap);

    auto *descLabel = infoLabel("DESCRIPTION", detailWrap);
    m_instDesc = new QTextEdit(detailWrap);
    m_instDesc->setReadOnly(true);
    m_instDesc->setFixedHeight(70);
    m_instDesc->setPlaceholderText("select a plugin");
    m_instDesc->setStyleSheet("background:rgba(8,8,8,0.85); color:#888; border:none; font-family:monospace; font-size:11px; padding:8px;");

    auto *howLabel = infoLabel("HOW TO USE", detailWrap);
    m_instHowTo = new QTextEdit(detailWrap);
    m_instHowTo->setReadOnly(true);
    m_instHowTo->setFixedHeight(60);
    m_instHowTo->setStyleSheet("background:rgba(8,8,8,0.85); color:#888; border:none; font-family:monospace; font-size:11px; padding:8px;");

    m_instRestart = valueLabel("", "#ffaa00", detailWrap);
    m_instPerms   = valueLabel("", "#555", detailWrap);

    auto *btnRow = new QWidget(detailWrap);
    btnRow->setStyleSheet("background:transparent;");
    auto *br = new QHBoxLayout(btnRow);
    br->setContentsMargins(0,0,0,0);
    br->setSpacing(10);
    m_toggleBtn    = pBtn("DISABLE", "#ffaa00", btnRow);
    m_uninstallBtn = pBtn("UNINSTALL", "#ff4444", btnRow);
    m_toggleBtn->setEnabled(false);
    m_uninstallBtn->setEnabled(false);
    br->addWidget(m_toggleBtn);
    br->addWidget(m_uninstallBtn);
    br->addStretch();

    dv->addWidget(m_instName);
    dv->addWidget(m_instAuthor);
    dv->addWidget(m_instVersion);
    dv->addWidget(descLabel);
    dv->addWidget(m_instDesc);
    dv->addWidget(howLabel);
    dv->addWidget(m_instHowTo);
    dv->addWidget(m_instRestart);
    dv->addWidget(m_instPerms);
    dv->addStretch();
    dv->addWidget(btnRow);

    il->addWidget(detailWrap, 1);

    connect(m_installedList, &QListWidget::currentRowChanged, this, &PluginsTab::onInstalledSelected);
    connect(m_toggleBtn,    &QPushButton::clicked, this, &PluginsTab::onTogglePlugin);
    connect(m_uninstallBtn, &QPushButton::clicked, this, &PluginsTab::onUninstall);

    // ════════════════════════════════════════════════════
    //  COMMUNITY tab
    // ════════════════════════════════════════════════════
    auto *comWidget = new QWidget;
    comWidget->setStyleSheet(panelStyle());
    auto *cl = new QHBoxLayout(comWidget);
    cl->setContentsMargins(0,0,0,0);
    cl->setSpacing(0);

    auto *comListWrap = new QWidget;
    comListWrap->setFixedWidth(260);
    comListWrap->setStyleSheet("border-right:1px solid #1a1a1a;");
    auto *clw = new QVBoxLayout(comListWrap);
    clw->setContentsMargins(0,0,0,0);
    clw->setSpacing(0);

    auto *comHdr = new QWidget;
    comHdr->setFixedHeight(30);
    comHdr->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *chl = new QHBoxLayout(comHdr);
    chl->setContentsMargins(10,0,10,0);
    chl->addWidget(infoLabel("COMMUNITY PLUGINS", comHdr));
    chl->addStretch();
    clw->addWidget(comHdr);

    m_communityList = new QListWidget(comListWrap);
    clw->addWidget(m_communityList, 1);

    m_refreshBtn = pBtn("↺ REFRESH", "#0088ff", comListWrap);
    m_refreshBtn->setStyleSheet(m_refreshBtn->styleSheet() + "border-radius:0; border-top:1px solid #1a1a1a;");
    connect(m_refreshBtn, &QPushButton::clicked, this, &PluginsTab::onRefreshCommunity);
    clw->addWidget(m_refreshBtn);
    cl->addWidget(comListWrap);

    auto *comDetailWrap = new QWidget;
    comDetailWrap->setStyleSheet("background:#0d0d0d;");
    auto *cdv = new QVBoxLayout(comDetailWrap);
    cdv->setContentsMargins(20,20,20,20);
    cdv->setSpacing(12);

    m_comName    = valueLabel("Browse community plugins", "#00cc66", comDetailWrap);
    m_comName->setStyleSheet("color:#00cc66; font-family:monospace; font-size:14px; font-weight:bold;");
    m_comAuthor  = valueLabel("—", "#555", comDetailWrap);
    m_comVersion = valueLabel("—", "#333", comDetailWrap);

    auto *cdescLabel = infoLabel("DESCRIPTION", comDetailWrap);
    m_comDesc = new QTextEdit(comDetailWrap);
    m_comDesc->setReadOnly(true);
    m_comDesc->setFixedHeight(70);
    m_comDesc->setPlaceholderText("click refresh to load community plugins");

    auto *chowLabel = infoLabel("HOW TO USE", comDetailWrap);
    m_comHowTo = new QTextEdit(comDetailWrap);
    m_comHowTo->setReadOnly(true);
    m_comHowTo->setFixedHeight(60);

    m_comRestart = valueLabel("", "#ffaa00", comDetailWrap);
    m_comPerms   = valueLabel("", "#555", comDetailWrap);
    m_comStatus  = valueLabel("", "#00cc66", comDetailWrap);

    auto *comBtnRow = new QWidget(comDetailWrap);
    comBtnRow->setStyleSheet("background:transparent;");
    auto *cbr = new QHBoxLayout(comBtnRow);
    cbr->setContentsMargins(0,0,0,0);
    cbr->setSpacing(10);
    m_installBtn = pBtn("↓ INSTALL", "#00cc66", comBtnRow);
    m_installBtn->setEnabled(false);
    cbr->addWidget(m_installBtn);
    cbr->addStretch();

    cdv->addWidget(m_comName);
    cdv->addWidget(m_comAuthor);
    cdv->addWidget(m_comVersion);
    cdv->addWidget(cdescLabel);
    cdv->addWidget(m_comDesc);
    cdv->addWidget(chowLabel);
    cdv->addWidget(m_comHowTo);
    cdv->addWidget(m_comRestart);
    cdv->addWidget(m_comPerms);
    cdv->addStretch();
    cdv->addWidget(m_comStatus);
    cdv->addWidget(comBtnRow);

    cl->addWidget(comDetailWrap, 1);

    connect(m_communityList, &QListWidget::currentRowChanged, this, &PluginsTab::onCommunitySelected);
    connect(m_installBtn, &QPushButton::clicked, this, &PluginsTab::onInstallFromRepo);

    m_tabs->addTab(instWidget, "INSTALLED");
    m_tabs->addTab(comWidget,  "COMMUNITY");

    root->addWidget(m_tabs, 1);
}

void PluginsTab::refreshInstalledList() {
    m_installedList->clear();
    for (auto &pm : PluginManager::instance().installedPlugins()) {
        QString label = pm.name + "\n" + pm.author + "  v" + pm.version;
        auto *item = new QListWidgetItem(label, m_installedList);
        item->setForeground(pm.enabled ? QColor("#00cc66") : QColor("#444"));
        item->setSizeHint(QSize(0, 48));
    }
    m_toggleBtn->setEnabled(false);
    m_uninstallBtn->setEnabled(false);
    m_instName->setText("No plugin selected");
}

void PluginsTab::onInstalledSelected(int row) {
    auto plugins = PluginManager::instance().installedPlugins();
    if (row < 0 || row >= plugins.size()) return;
    const auto &pm = plugins[row];
    m_instName->setText(pm.name);
    m_instAuthor->setText("by " + pm.author);
    m_instVersion->setText("v" + pm.version);
    m_instDesc->setPlainText(pm.description);
    m_instHowTo->setPlainText(pm.howToUse);
    m_instRestart->setText(pm.requiresRestart ? "⚠  Requires browser restart after toggle" : "");
    m_instPerms->setText(pm.permissions.isEmpty() ? "" : "Permissions: " + pm.permissions.join(", "));
    m_toggleBtn->setText(pm.enabled ? "DISABLE" : "ENABLE");
    m_toggleBtn->setEnabled(true);
    m_uninstallBtn->setEnabled(true);
}

void PluginsTab::onTogglePlugin() {
    int row = m_installedList->currentRow();
    auto plugins = PluginManager::instance().installedPlugins();
    if (row < 0 || row >= plugins.size()) return;
    const auto &pm = plugins[row];
    bool nowEnabled = !pm.enabled;
    PluginManager::instance().setEnabled(pm.id, nowEnabled);
    refreshInstalledList();
    m_installedList->setCurrentRow(row);
    if (pm.requiresRestart)
        QMessageBox::information(this, "Restart Required",
            QString("'%1' requires a browser restart to take effect.").arg(pm.name));
}

void PluginsTab::onUninstall() {
    int row = m_installedList->currentRow();
    auto plugins = PluginManager::instance().installedPlugins();
    if (row < 0 || row >= plugins.size()) return;
    auto res = QMessageBox::question(this, "Uninstall Plugin",
        "Uninstall '" + plugins[row].name + "'?",
        QMessageBox::Yes | QMessageBox::No);
    if (res != QMessageBox::Yes) return;
    PluginManager::instance().uninstallPlugin(plugins[row].id);
}

void PluginsTab::onRefreshCommunity() {
    m_refreshBtn->setEnabled(false);
    m_communityList->clear();
    auto *loadingItem = new QListWidgetItem("loading...", m_communityList);
    loadingItem->setForeground(QColor("#333"));
    PluginManager::instance().fetchCommunityList();
}

void PluginsTab::onCommunityListReady(const QList<PluginManifest> &list) {
    m_refreshBtn->setEnabled(true);
    m_communityList->clear();
    m_communityCache = list;
    for (auto &pm : list) {
        QString label = pm.name + "\n" + pm.author + "  v" + pm.version
                        + (pm.installed ? "  [INSTALLED]" : "");
        auto *item = new QListWidgetItem(label, m_communityList);
        item->setForeground(pm.installed ? QColor("#00cc66") : QColor("#aaa"));
        item->setSizeHint(QSize(0, 48));
    }
    if (list.isEmpty()) {
        auto *empty = new QListWidgetItem("no community plugins found", m_communityList);
        empty->setForeground(QColor("#333"));
    }
}

void PluginsTab::onCommunitySelected(int row) {
    if (row < 0 || row >= m_communityCache.size()) return;
    const auto &pm = m_communityCache[row];
    m_comName->setText(pm.name);
    m_comAuthor->setText("by " + pm.author);
    m_comVersion->setText("v" + pm.version);
    m_comDesc->setPlainText(pm.description);
    m_comHowTo->setPlainText(pm.howToUse);
    m_comRestart->setText(pm.requiresRestart ? "⚠  Requires browser restart after install" : "");
    m_comPerms->setText(pm.permissions.isEmpty() ? "" : "Permissions: " + pm.permissions.join(", "));
    m_comStatus->setText("");
    m_installBtn->setEnabled(!pm.installed);
    m_installBtn->setText(pm.installed ? "INSTALLED" : "↓ INSTALL");
}

void PluginsTab::onInstallFromRepo() {
    int row = m_communityList->currentRow();
    if (row < 0 || row >= m_communityCache.size()) return;
    const auto &pm = m_communityCache[row];
    m_installBtn->setEnabled(false);
    m_comStatus->setText("installing...");
    PluginManager::instance().installFromRepo(pm.id);
}

void PluginsTab::onInstallComplete(const QString &id, bool ok, const QString &err) {
    m_comStatus->setText(ok ? "✓ installed" : "✕ failed: " + err);
    m_comStatus->setStyleSheet(ok
        ? "color:#00cc66; font-family:monospace; font-size:11px;"
        : "color:#ff4444; font-family:monospace; font-size:11px;");
    if (ok) {
        refreshInstalledList();
        auto plugins = PluginManager::instance().installedPlugins();
        bool needsRestart = false;
        for (auto &p : plugins) if (p.id == id && p.requiresRestart) { needsRestart = true; break; }
        if (needsRestart)
            QMessageBox::information(this, "Restart Required",
                "This plugin requires a browser restart to activate.");
    }
    onRefreshCommunity();
}

void PluginsTab::refreshCommunityList() {}