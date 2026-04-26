#include "ProxyPanel.h"
#include "../engine/ProxyManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QGroupBox>
#include <QFrame>
#include <QHeaderView>
#include <QDir>

// ── Style helpers ─────────────────────────────────────────────────────────────
static QString btnStyle(const QString &accent) {
    auto bg  = [&](int a) {
        if (accent == "#ff4444") return QString("rgba(80,0,0,%1)").arg(a);
        if (accent == "#ffaa44") return QString("rgba(80,50,0,%1)").arg(a);
        if (accent == "#0088ff") return QString("rgba(0,40,80,%1)").arg(a);
        return QString("rgba(0,60,40,%1)").arg(a);
    };
    return QString(R"(
        QPushButton {
            background:%1; color:%2;
            border:1px solid %3;
            border-radius:3px;
            font-family:"Courier New",monospace;
            font-size:11px; letter-spacing:1px;
            padding:5px 10px;
        }
        QPushButton:hover   { background:%4; border-color:%2; }
        QPushButton:pressed { background:#000; }
        QPushButton:disabled{ color:#333; border-color:#1a1a1a; background:#050505; }
    )")
    .arg(bg(80), accent, bg(120).replace("rgba","rgba"), bg(120));
}

static QString inputStyle() {
    return R"(
        QLineEdit,QSpinBox,QComboBox {
            background:#0d0d0d; color:#ccc;
            border:1px solid #222; border-radius:3px;
            padding:4px 8px;
            font-family:"Courier New",monospace; font-size:11px;
        }
        QLineEdit:focus,QSpinBox:focus,QComboBox:focus { border-color:#00ff8844; }
        QComboBox::drop-down { border:none; }
        QComboBox::down-arrow {
            border-left:4px solid transparent; border-right:4px solid transparent;
            border-top:5px solid #555; width:0; height:0; margin-right:6px;
        }
        QComboBox QAbstractItemView {
            background:#0d0d0d; color:#ccc; border:1px solid #222;
            selection-background-color:#00ff8822;
        }
        QSpinBox::up-button,QSpinBox::down-button {
            background:#151515; border:1px solid #1e1e1e; width:16px;
        }
        QSpinBox::up-arrow {
            border-left:3px solid transparent; border-right:3px solid transparent;
            border-bottom:4px solid #555; width:0; height:0;
        }
        QSpinBox::down-arrow {
            border-left:3px solid transparent; border-right:3px solid transparent;
            border-top:4px solid #555; width:0; height:0;
        }
    )";
}

static QFrame *divider() {
    auto *f = new QFrame;
    f->setFrameShape(QFrame::HLine);
    f->setStyleSheet("color:#1a1a1a;background:#1a1a1a;border:none;max-height:1px;margin:4px 0;");
    return f;
}

static QLabel *sectionLabel(const QString &t) {
    auto *l = new QLabel(t);
    l->setStyleSheet(
        "color:#00ff8877;font-family:'Courier New',monospace;"
        "font-size:10px;letter-spacing:2px;padding:2px 0;");
    return l;
}

// ═════════════════════════════════════════════════════════════════════════════

ProxyPanel::ProxyPanel(QWidget *parent) : QWidget(parent) {
    setStyleSheet(R"(
        QWidget { background:#0a0a0a; }
        QGroupBox {
            background:#111; border:1px solid #1e1e1e;
            border-radius:4px; margin-top:6px;
            font-family:"Courier New",monospace;
            font-size:10px; color:#444; letter-spacing:1px;
        }
        QGroupBox::title {
            subcontrol-origin:margin; left:10px;
            padding:0 4px; color:#00ff8866;
        }
        QCheckBox {
            color:#555; font-family:"Courier New",monospace; font-size:11px;
        }
        QCheckBox:checked { color:#00ff88; }
        QCheckBox::indicator { width:11px; height:11px; }
        QTableWidget {
            background:#0d0d0d; color:#ccc;
            font-family:"Courier New",monospace; font-size:10px;
            border:none; gridline-color:#161616;
            selection-background-color:#0a2a0a;
        }
        QTableWidget::item { padding:2px 4px; border-bottom:1px solid #141414; }
        QHeaderView::section {
            background:#090909; color:#3a3a3a;
            font-family:"Courier New",monospace; font-size:10px;
            border:none; border-right:1px solid #1a1a1a;
            border-bottom:1px solid #1e1e1e; padding:2px 4px;
        }
        QProgressBar {
            background:#111; border:1px solid #1e1e1e;
            border-radius:2px; height:6px; text-align:center;
            font-family:"Courier New",monospace; font-size:9px; color:#00ff8888;
        }
        QProgressBar::chunk { background:#00ff88; border-radius:2px; }
        QScrollBar:vertical { background:#090909; width:5px; border:none; }
        QScrollBar::handle:vertical { background:#1e1e1e; border-radius:2px; min-height:16px; }
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
    )");

    buildUI();

    auto &pm = ProxyManager::instance();
    connect(&pm, &ProxyManager::proxyChanged,   this, &ProxyPanel::onProxyChanged);
    connect(&pm, &ProxyManager::proxyListLoaded, this, &ProxyPanel::onListLoaded);
    connect(&pm, &ProxyManager::fetchFailed,    this, &ProxyPanel::onFetchFailed);
    connect(&pm, &ProxyManager::ovpnLoaded,     this, &ProxyPanel::onOvpnLoaded);
    connect(&pm, &ProxyManager::checkStarted,   this, &ProxyPanel::onCheckStarted);
    connect(&pm, &ProxyManager::checkProgress,  this, &ProxyPanel::onCheckProgress);
    connect(&pm, &ProxyManager::checkFinished,  this, &ProxyPanel::onCheckFinished);
}

// ─────────────────────────────────────────────────────────────────────────────
void ProxyPanel::buildUI() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10,10,10,10);
    root->setSpacing(8);

    // ── STATUS ────────────────────────────────────────────────────────────────
    auto *statusRow = new QHBoxLayout; statusRow->setSpacing(7);
    m_statusDot  = new QLabel("●");
    m_statusDot->setFixedWidth(14);
    m_statusDot->setStyleSheet("color:#333;font-size:10px;");
    m_statusText = new QLabel("NO PROXY");
    m_statusText->setStyleSheet(
        "color:#444;font-family:'Courier New',monospace;font-size:11px;letter-spacing:2px;");
    m_countLabel = new QLabel("");
    m_countLabel->setStyleSheet(
        "color:#00ff8855;font-family:'Courier New',monospace;font-size:10px;");
    m_countLabel->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    statusRow->addWidget(m_statusDot);
    statusRow->addWidget(m_statusText,1);
    statusRow->addWidget(m_countLabel);
    root->addLayout(statusRow);

    m_currentLabel = new QLabel("—");
    m_currentLabel->setWordWrap(true);
    m_currentLabel->setStyleSheet(
        "color:#335544;font-family:'Courier New',monospace;font-size:10px;"
        "padding:4px 8px;background:#0d0d0d;border:1px solid #151515;border-radius:3px;");
    root->addWidget(m_currentLabel);
    root->addWidget(divider());

    // ── SOURCE ────────────────────────────────────────────────────────────────
    root->addWidget(sectionLabel("SOURCE"));
    auto *srcGroup = new QGroupBox("Proxy List");
    auto *srcGrid  = new QGridLayout(srcGroup);
    srcGrid->setContentsMargins(8,14,8,8); srcGrid->setSpacing(6);

    m_loadTxtBtn  = new QPushButton("↑ LOAD .TXT",  srcGroup);
    m_loadOvpnBtn = new QPushButton("↑ LOAD .OVPN", srcGroup);
    m_loadTxtBtn->setStyleSheet(btnStyle("#00ff88"));
    m_loadOvpnBtn->setStyleSheet(btnStyle("#ffaa44"));

    m_fetchUrlEdit = new QLineEdit(srcGroup);
    m_fetchUrlEdit->setPlaceholderText("https://example.com/proxies.txt");
    m_fetchUrlEdit->setStyleSheet(inputStyle());
    m_fetchBtn = new QPushButton("FETCH", srcGroup);
    m_fetchBtn->setFixedWidth(60);
    m_fetchBtn->setStyleSheet(btnStyle("#00ff88"));

    m_ovpnLabel = new QLabel("", srcGroup);
    m_ovpnLabel->setWordWrap(true);
    m_ovpnLabel->setStyleSheet(
        "color:#aa6600;font-family:'Courier New',monospace;font-size:10px;");

    auto *fetchRow = new QHBoxLayout;
    fetchRow->addWidget(m_fetchUrlEdit,1);
    fetchRow->addWidget(m_fetchBtn);

    srcGrid->addWidget(m_loadTxtBtn,  0, 0);
    srcGrid->addWidget(m_loadOvpnBtn, 0, 1);
    srcGrid->addLayout(fetchRow,       1, 0, 1, 2);
    srcGrid->addWidget(m_ovpnLabel,    2, 0, 1, 2);
    root->addWidget(srcGroup);
    root->addWidget(divider());

    // ── HEALTH CHECKER ────────────────────────────────────────────────────────
    root->addWidget(sectionLabel("HEALTH CHECKER"));
    auto *hcGroup = new QGroupBox("Check Proxies");
    auto *hcLayout = new QVBoxLayout(hcGroup);
    hcLayout->setContentsMargins(8,14,8,8); hcLayout->setSpacing(6);

    // buttons row
    auto *hcBtnRow = new QHBoxLayout; hcBtnRow->setSpacing(6);
    m_checkAllBtn  = new QPushButton("▶ CHECK ALL", hcGroup);
    m_stopCheckBtn = new QPushButton("■ STOP",      hcGroup);
    m_checkAllBtn->setStyleSheet(btnStyle("#00ff88"));
    m_stopCheckBtn->setStyleSheet(btnStyle("#ff4444"));
    m_stopCheckBtn->setEnabled(false);
    hcBtnRow->addWidget(m_checkAllBtn);
    hcBtnRow->addWidget(m_stopCheckBtn);
    hcLayout->addLayout(hcBtnRow);

    // options row
    auto *hcOptRow = new QHBoxLayout; hcOptRow->setSpacing(12);
    m_autoCheckBox = new QCheckBox("Auto-check on load", hcGroup);
    m_skipDeadBox  = new QCheckBox("Skip dead on rotate", hcGroup);
    m_skipDeadBox->setChecked(true);
    hcOptRow->addWidget(m_autoCheckBox);
    hcOptRow->addWidget(m_skipDeadBox);
    hcOptRow->addStretch();
    hcLayout->addLayout(hcOptRow);

    // progress bar
    m_checkProgress = new QProgressBar(hcGroup);
    m_checkProgress->setRange(0,100);
    m_checkProgress->setValue(0);
    m_checkProgress->setFixedHeight(8);
    m_checkProgress->setTextVisible(false);
    hcLayout->addWidget(m_checkProgress);

    // stats label
    m_checkStats = new QLabel("—", hcGroup);
    m_checkStats->setStyleSheet(
        "color:#444;font-family:'Courier New',monospace;font-size:10px;");
    hcLayout->addWidget(m_checkStats);

    root->addWidget(hcGroup);
    root->addWidget(divider());

    // ── PROXY TABLE ───────────────────────────────────────────────────────────
    root->addWidget(sectionLabel("PROXY LIST"));
    m_proxyTable = new QTableWidget(0, 5, this);
    m_proxyTable->setHorizontalHeaderLabels({"HOST","PORT","TYPE","STATUS","PING"});
    m_proxyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_proxyTable->setColumnWidth(1, 48);
    m_proxyTable->setColumnWidth(2, 54);
    m_proxyTable->setColumnWidth(3, 72);
    m_proxyTable->setColumnWidth(4, 54);
    m_proxyTable->verticalHeader()->setVisible(false);
    m_proxyTable->setShowGrid(false);
    m_proxyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_proxyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_proxyTable->setMaximumHeight(220);
    connect(m_proxyTable, &QTableWidget::cellClicked,
            this, &ProxyPanel::onTableRowClicked);
    root->addWidget(m_proxyTable);
    root->addWidget(divider());

    // ── ROTATION ─────────────────────────────────────────────────────────────
    root->addWidget(sectionLabel("ROTATION"));
    auto *rotGroup = new QGroupBox("Mode");
    auto *rotRow   = new QHBoxLayout(rotGroup);
    rotRow->setContentsMargins(8,14,8,8); rotRow->setSpacing(8);
    m_rotationCombo = new QComboBox(rotGroup);
    m_rotationCombo->setStyleSheet(inputStyle());
    m_rotationCombo->addItem("Off — Use Current",  (int)ProxyRotation::None);
    m_rotationCombo->addItem("Per Request",        (int)ProxyRotation::PerRequest);
    m_rotationCombo->addItem("Timed Interval",     (int)ProxyRotation::Timed);
    m_intervalSpin = new QSpinBox(rotGroup);
    m_intervalSpin->setStyleSheet(inputStyle());
    m_intervalSpin->setRange(10,3600);
    m_intervalSpin->setValue(60);
    m_intervalSpin->setSuffix(" s");
    m_intervalSpin->setFixedWidth(80);
    m_intervalSpin->setEnabled(false);
    rotRow->addWidget(m_rotationCombo,1);
    rotRow->addWidget(m_intervalSpin);
    root->addWidget(rotGroup);

    // ── CONTROL ──────────────────────────────────────────────────────────────
    auto *ctrlRow = new QHBoxLayout; ctrlRow->setSpacing(6);
    m_nextBtn    = new QPushButton("→ NEXT",      this);
    m_disableBtn = new QPushButton("✕ DISABLE",   this);
    m_enableBtn  = new QPushButton("✓ RE-ENABLE", this);
    m_nextBtn->setStyleSheet(btnStyle("#00ff88"));
    m_disableBtn->setStyleSheet(btnStyle("#ff4444"));
    m_enableBtn->setStyleSheet(btnStyle("#00ff88"));
    ctrlRow->addWidget(m_nextBtn);
    ctrlRow->addWidget(m_disableBtn);
    ctrlRow->addWidget(m_enableBtn);
    root->addLayout(ctrlRow);
    root->addStretch();

    // hint
    auto *hint = new QLabel(
        "txt: socks5://host:port  ·  http://host:port  ·  host:port  ·  host:port:user:pass");
    hint->setStyleSheet("color:#1e1e1e;font-family:'Courier New',monospace;font-size:9px;");
    root->addWidget(hint);

    // ── Wire ─────────────────────────────────────────────────────────────────
    connect(m_loadTxtBtn,    &QPushButton::clicked,  this, &ProxyPanel::onLoadTxt);
    connect(m_loadOvpnBtn,   &QPushButton::clicked,  this, &ProxyPanel::onLoadOvpn);
    connect(m_fetchBtn,      &QPushButton::clicked,  this, &ProxyPanel::onFetchUrl);
    connect(m_checkAllBtn,   &QPushButton::clicked,  this, &ProxyPanel::onCheckAll);
    connect(m_stopCheckBtn,  &QPushButton::clicked,  this, &ProxyPanel::onStopCheck);
    connect(m_nextBtn,       &QPushButton::clicked,  this, &ProxyPanel::onNext);
    connect(m_disableBtn,    &QPushButton::clicked,  this, &ProxyPanel::onDisable);
    connect(m_enableBtn,     &QPushButton::clicked,  this,
        []{ ProxyManager::instance().enableCurrent(); });
    connect(m_rotationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProxyPanel::onRotationChanged);
    connect(m_autoCheckBox,  &QCheckBox::toggled,
        [](bool on){ ProxyManager::instance().setAutoCheck(on); });
    connect(m_skipDeadBox,   &QCheckBox::toggled,
        [](bool on){ ProxyManager::instance().setSkipDead(on); });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Proxy table
// ─────────────────────────────────────────────────────────────────────────────
void ProxyPanel::refreshProxyTable() {
    auto proxies = ProxyManager::instance().proxies();
    m_proxyTable->setRowCount(0);

    for (int i = 0; i < proxies.size(); i++) {
        const auto &e = proxies[i];
        m_proxyTable->insertRow(i);
        m_proxyTable->setRowHeight(i, 18);

        // Host
        auto *hostItem = new QTableWidgetItem(e.host);
        hostItem->setForeground(QColor("#cccccc"));
        m_proxyTable->setItem(i, 0, hostItem);

        // Port
        auto *portItem = new QTableWidgetItem(QString::number(e.port));
        portItem->setForeground(QColor("#666"));
        m_proxyTable->setItem(i, 1, portItem);

        // Type
        QString typeStr = e.type == ProxyEntry::SOCKS5 ? "S5" :
                          e.type == ProxyEntry::HTTPS  ? "HTTPS" : "HTTP";
        auto *typeItem = new QTableWidgetItem(typeStr);
        typeItem->setForeground(QColor("#555"));
        m_proxyTable->setItem(i, 2, typeItem);

        // Status
        QString statusStr;
        QColor  statusCol;
        switch (e.health) {
            case ProxyEntry::Unchecked: statusStr = "—";        statusCol = "#444";    break;
            case ProxyEntry::Checking:  statusStr = "checking"; statusCol = "#ffaa44"; break;
            case ProxyEntry::Alive:     statusStr = "alive";    statusCol = "#00ff88"; break;
            case ProxyEntry::Dead:      statusStr = "dead";     statusCol = "#ff4444"; break;
        }
        auto *statusItem = new QTableWidgetItem(statusStr);
        statusItem->setForeground(statusCol);
        m_proxyTable->setItem(i, 3, statusItem);

        // Latency
        QString latStr = e.latency >= 0 ? QString::number(e.latency) + "ms" : "—";
        auto *latItem  = new QTableWidgetItem(latStr);
        QColor latCol  = e.latency < 0    ? QColor("#333") :
                         e.latency < 500  ? QColor("#00ff88") :
                         e.latency < 1500 ? QColor("#ffaa44") : QColor("#ff4444");
        latItem->setForeground(latCol);
        m_proxyTable->setItem(i, 4, latItem);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Source slots
// ─────────────────────────────────────────────────────────────────────────────
void ProxyPanel::onLoadTxt() {
    QString path = QFileDialog::getOpenFileName(
        this, "Load Proxy List", QDir::homePath(),
        "Text files (*.txt);;All files (*)");
    if (path.isEmpty()) return;
    bool ok = ProxyManager::instance().loadFromFile(path);
    if (!ok) {
        m_statusText->setText("LOAD FAILED");
        m_statusDot->setStyleSheet("color:#ff4444;font-size:10px;");
    }
}

void ProxyPanel::onLoadOvpn() {
    QString path = QFileDialog::getOpenFileName(
        this, "Load ProtonVPN Config", QDir::homePath(),
        "OpenVPN config (*.ovpn);;All files (*)");
    if (path.isEmpty()) return;
    bool ok = ProxyManager::instance().loadOvpnFile(path);
    if (!ok) {
        m_ovpnLabel->setText("✕ Could not parse .ovpn file");
        m_ovpnLabel->setStyleSheet(
            "color:#ff4444;font-family:'Courier New',monospace;font-size:10px;");
    }
}

void ProxyPanel::onFetchUrl() {
    QString url = m_fetchUrlEdit->text().trimmed();
    if (url.isEmpty()) return;
    if (!url.startsWith("http")) url.prepend("https://");
    m_statusText->setText("FETCHING...");
    m_statusDot->setStyleSheet("color:#ffaa44;font-size:10px;");
    m_fetchBtn->setEnabled(false);
    ProxyManager::instance().fetchFromUrl(url);
    QTimer::singleShot(15000, this, [this]{ m_fetchBtn->setEnabled(true); });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Health checker slots
// ─────────────────────────────────────────────────────────────────────────────
void ProxyPanel::onCheckAll() {
    if (ProxyManager::instance().count() == 0) return;
    m_checkAllBtn->setEnabled(false);
    m_stopCheckBtn->setEnabled(true);
    ProxyManager::instance().checkAll();
}

void ProxyPanel::onStopCheck() {
    ProxyManager::instance().stopChecking();
    m_checkAllBtn->setEnabled(true);
    m_stopCheckBtn->setEnabled(false);
    m_checkStats->setText(m_checkStats->text() + "  [stopped]");
}

void ProxyPanel::onCheckStarted(int total) {
    m_checkTotal = total;
    m_checkDone  = 0;
    m_checkProgress->setRange(0, total);
    m_checkProgress->setValue(0);
    m_checkStats->setText(QString("checking 0 / %1  |  alive: 0  |  dead: 0").arg(total));
    m_checkStats->setStyleSheet(
        "color:#ffaa44;font-family:'Courier New',monospace;font-size:10px;");
    refreshProxyTable();
}

void ProxyPanel::onCheckProgress(int index, ProxyEntry::Health result, int latencyMs) {
    m_checkDone++;
    m_checkProgress->setValue(m_checkDone);

    // Update just that row in the table — no full refresh (too slow for 500 proxies)
    if (index < m_proxyTable->rowCount()) {
        QString statusStr;
        QColor  statusCol;
        switch (result) {
            case ProxyEntry::Alive: statusStr = "alive"; statusCol = "#00ff88"; break;
            case ProxyEntry::Dead:  statusStr = "dead";  statusCol = "#ff4444"; break;
            default:                statusStr = "—";     statusCol = "#444";    break;
        }
        auto *si = m_proxyTable->item(index, 3);
        if (si) { si->setText(statusStr); si->setForeground(statusCol); }

        QString latStr = latencyMs >= 0 ? QString::number(latencyMs)+"ms" : "—";
        QColor latCol  = latencyMs < 0    ? QColor("#333") :
                         latencyMs < 500  ? QColor("#00ff88") :
                         latencyMs < 1500 ? QColor("#ffaa44") : QColor("#ff4444");
        auto *li = m_proxyTable->item(index, 4);
        if (li) { li->setText(latStr); li->setForeground(latCol); }
    }

    // Update stats label
    int alive = ProxyManager::instance().aliveCount();
    int dead  = ProxyManager::instance().deadCount();
    m_checkStats->setText(
        QString("checked %1 / %2  |  alive: %3  |  dead: %4")
            .arg(m_checkDone).arg(m_checkTotal).arg(alive).arg(dead));
}

void ProxyPanel::onCheckFinished(int alive, int dead) {
    m_checkAllBtn->setEnabled(true);
    m_stopCheckBtn->setEnabled(false);
    m_checkProgress->setValue(m_checkTotal);
    m_checkStats->setText(
        QString("done — alive: %1  |  dead: %2  |  total: %3")
            .arg(alive).arg(dead).arg(m_checkTotal));
    m_checkStats->setStyleSheet(
        "color:#00ff88;font-family:'Courier New',monospace;font-size:10px;");
    m_countLabel->setText(
        QString("%1 alive / %2").arg(alive).arg(m_checkTotal));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Table row click → use that proxy immediately
// ─────────────────────────────────────────────────────────────────────────────
void ProxyPanel::onTableRowClicked(int row, int /*col*/) {
    auto proxies = ProxyManager::instance().proxies();
    if (row < 0 || row >= proxies.size()) return;
    // Only switch if not dead (or skipDead is off)
    const auto &e = proxies[row];
    if (e.health == ProxyEntry::Dead &&
        ProxyManager::instance().skipDead()) return;
    // Tell ProxyManager to jump to this index
    // We do it by calling next() until we reach it — crude but safe
    // Better: expose setIndex() — add this to ProxyManager if needed
    // For now just apply directly
    QNetworkProxy::setApplicationProxy(e.toQProxy());
    // Update status display manually
    onProxyChanged(e);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Other slots
// ─────────────────────────────────────────────────────────────────────────────
void ProxyPanel::onNext() { ProxyManager::instance().next(); }

void ProxyPanel::onDisable() {
    ProxyManager::instance().disable();
    m_statusDot->setStyleSheet("color:#333;font-size:10px;");
    m_statusText->setText("DISABLED");
    m_currentLabel->setText("—");
    m_currentLabel->setStyleSheet(
        "color:#335544;font-family:'Courier New',monospace;font-size:10px;"
        "padding:4px 8px;background:#0d0d0d;border:1px solid #151515;border-radius:3px;");
}

void ProxyPanel::onRotationChanged(int) {
    auto mode = (ProxyRotation)m_rotationCombo->currentData().toInt();
    m_intervalSpin->setEnabled(mode == ProxyRotation::Timed);
    ProxyManager::instance().setRotation(mode, m_intervalSpin->value());
}

void ProxyPanel::onProxyChanged(const ProxyEntry &e) {
    if (!e.isValid()) {
        m_statusDot->setStyleSheet("color:#333;font-size:10px;");
        m_statusText->setText("NO PROXY");
        m_currentLabel->setText("—");
        return;
    }
    m_statusDot->setStyleSheet("color:#00ff88;font-size:10px;");
    m_statusText->setText("ACTIVE");
    QString typeStr = e.type == ProxyEntry::SOCKS5 ? "SOCKS5" :
                      e.type == ProxyEntry::HTTPS  ? "HTTPS"  : "HTTP";
    QString display = typeStr + "  " + e.host + ":" + QString::number(e.port);
    if (e.latency >= 0) display += "  " + QString::number(e.latency) + "ms";
    if (!e.user.isEmpty()) display += "  [auth]";
    m_currentLabel->setText(display);
    m_currentLabel->setStyleSheet(
        "color:#00ff88;font-family:'Courier New',monospace;font-size:10px;"
        "padding:4px 8px;background:#0d1a10;border:1px solid #00ff8822;border-radius:3px;");
}

void ProxyPanel::onListLoaded(int count) {
    m_fetchBtn->setEnabled(true);
    m_countLabel->setText(QString::number(count) + " proxies");
    m_statusDot->setStyleSheet("color:#00ff88;font-size:10px;");
    m_statusText->setText("LOADED");
    m_checkStats->setText("— run CHECK ALL to verify proxies");
    m_checkStats->setStyleSheet(
        "color:#444;font-family:'Courier New',monospace;font-size:10px;");
    refreshProxyTable();
}

void ProxyPanel::onFetchFailed(const QString &err) {
    m_fetchBtn->setEnabled(true);
    m_statusDot->setStyleSheet("color:#ff4444;font-size:10px;");
    m_statusText->setText("FETCH ERR");
    m_currentLabel->setText(err.left(80));
    m_currentLabel->setStyleSheet(
        "color:#ff4444;font-family:'Courier New',monospace;font-size:10px;"
        "padding:4px 8px;background:#1a0000;border:1px solid #330000;border-radius:3px;");
}

void ProxyPanel::onOvpnLoaded(const QString &remote, int port) {
    m_ovpnLabel->setText(
        QString("✓ remote: %1:%2\n"
                "  → via ProtonVPN local SOCKS5 (127.0.0.1:1080)\n"
                "    requires ProtonVPN app running in background.")
        .arg(remote).arg(port));
    m_ovpnLabel->setStyleSheet(
        "color:#aa6600;font-family:'Courier New',monospace;font-size:10px;");
    refreshProxyTable();
}