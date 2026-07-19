#include "DevToolsPanel.h"
#include "ProxyPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QMessageBox>

// ═════════════════════════════════════════════════════════════════════════════
//  Construction
// ═════════════════════════════════════════════════════════════════════════════
DevToolsPanel::DevToolsPanel(QWidget *parent) : QWidget(parent) {
    setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildNetworkTab(), "NETWORK [0]");
    m_tabs->addTab(buildWsTab(),      "WS [0]");
    m_tabs->addTab(buildCookiesTab(), "COOKIES [0]");
    m_tabs->addTab(buildStorageTab(), "STORAGE [0]");
    m_tabs->addTab(buildExportTab(),  "EXPORT");
    m_tabs->addTab(new ProxyPanel(this), "PROXY");   // ← proxy tab
    root->addWidget(m_tabs);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Network Tab
// ═════════════════════════════════════════════════════════════════════════════
QWidget *DevToolsPanel::buildNetworkTab() {
    auto *w = new QWidget; w->setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(w);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    auto *bar = new QWidget; bar->setFixedHeight(34);
    bar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *bl = new QHBoxLayout(bar);
    bl->setContentsMargins(8,3,8,3); bl->setSpacing(6);
    m_netCount = new QLabel("CAPTURED: 0", bar); m_netCount->setObjectName("cnt");
    m_netFilter = new QLineEdit(bar); m_netFilter->setPlaceholderText("filter url...");
    m_netFilter->setFixedWidth(180);
    m_typeFilter = new QComboBox(bar);
    m_typeFilter->addItems({"All","XHR","Fetch","WS","Script","Doc","Img","Other"});
    m_typeFilter->setFixedWidth(75);
    auto *clearBtn = btn("CLEAR", "#ff4444", bar); clearBtn->setFixedWidth(55);
    connect(clearBtn, &QPushButton::clicked, this, &DevToolsPanel::clearAll);
    connect(m_netFilter,  &QLineEdit::textChanged, this, &DevToolsPanel::filterNetwork);
    connect(m_typeFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int){ filterNetwork(m_netFilter->text()); });
    bl->addWidget(m_netCount); bl->addStretch();
    bl->addWidget(new QLabel("filter:", bar));
    bl->addWidget(m_netFilter); bl->addWidget(m_typeFilter); bl->addWidget(clearBtn);

    auto *splitter = new QSplitter(Qt::Horizontal, w);

    m_netTable = new QTableWidget(0, 5, splitter);
    m_netTable->setHorizontalHeaderLabels({"MTH","STATUS","TYPE","SIZE","URL"});
    m_netTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_netTable->setColumnWidth(0,48); m_netTable->setColumnWidth(1,48);
    m_netTable->setColumnWidth(2,52); m_netTable->setColumnWidth(3,52);
    m_netTable->verticalHeader()->setVisible(false);
    m_netTable->setShowGrid(false);
    m_netTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_netTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_netTable, &QTableWidget::cellClicked, this, &DevToolsPanel::onNetworkRowSelected);

    auto *rp = new QWidget(splitter); rp->setStyleSheet(panelStyle());
    auto *rl = new QVBoxLayout(rp); rl->setContentsMargins(0,0,0,0); rl->setSpacing(0);

    auto *copyBar = new QWidget; copyBar->setFixedHeight(32);
    copyBar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *cbl = new QHBoxLayout(copyBar); cbl->setContentsMargins(6,2,6,2); cbl->setSpacing(4);

    auto *bCpHdr  = btn("COPY HEADERS",  "#888888", copyBar);
    auto *bCpResp = btn("COPY RESPONSE", "#888888", copyBar);
    auto *bCurl   = btn("AS CURL",       "#0088ff", copyBar);
    auto *bPython = btn("AS PYTHON",     "#00cc66", copyBar);
    auto *bDl     = btn("DOWNLOAD",      "#ffaa00", copyBar);

    connect(bCpHdr, &QPushButton::clicked, this, [this](){
        int r = m_netTable->currentRow();
        if (r >= 0 && r < m_netEntries.size())
            clip(buildSummaryBlock(m_netEntries[r]) + buildHeadersBlock(m_netEntries[r]));
    });
    connect(bCpResp, &QPushButton::clicked, this, [this](){
        int r = m_netTable->currentRow();
        if (r >= 0 && r < m_netEntries.size()) clip(m_netEntries[r].body);
    });
    connect(bCurl, &QPushButton::clicked, this, [this](){
        int r = m_netTable->currentRow();
        if (r >= 0 && r < m_netEntries.size()) {
            auto &e = m_netEntries[r];
            clip(generateCurl(e.method, e.url, e.reqHeaders, e.body, cookiesForUrl(e.url)));
        }
    });
    connect(bPython, &QPushButton::clicked, this, [this](){
        int r = m_netTable->currentRow();
        if (r >= 0 && r < m_netEntries.size()) {
            auto &e = m_netEntries[r];
            clip(generatePython(e.method, e.url, e.reqHeaders, e.body, cookiesForUrl(e.url)));
        }
    });
    connect(bDl, &QPushButton::clicked, this, &DevToolsPanel::downloadSelected);

    cbl->addWidget(bCpHdr); cbl->addWidget(bCpResp);
    cbl->addSpacing(6);
    cbl->addWidget(bCurl); cbl->addWidget(bPython);
    cbl->addSpacing(6);
    cbl->addWidget(bDl); cbl->addStretch();

    m_netDetailTabs = new QTabWidget(rp);
    m_netDetailTabs->setStyleSheet(
        "QTabBar::tab { padding:4px 10px; font-size:10px; background:#090909; color:#444; "
        "border:none; border-right:1px solid #1a1a1a; }"
        "QTabBar::tab:selected { color:#00cc66; border-bottom:1px solid #00cc66; background:#0d0d0d; }");
    m_netHeadersView  = new QTextEdit; m_netHeadersView->setReadOnly(true);
    m_netHeadersView->setPlaceholderText("// click a request");
    m_netResponseView = new QTextEdit; m_netResponseView->setReadOnly(true);
    m_netResponseView->setPlaceholderText("// click a request");
    m_netRawView      = new QTextEdit; m_netRawView->setReadOnly(true);
    m_netRawView->setPlaceholderText("// click a request");
    m_netDetailTabs->addTab(m_netHeadersView,  "Summary + Headers");
    m_netDetailTabs->addTab(m_netResponseView, "Response");
    m_netDetailTabs->addTab(m_netRawView,      "Raw");

    rl->addWidget(copyBar);
    rl->addWidget(m_netDetailTabs, 1);

    splitter->addWidget(m_netTable);
    splitter->addWidget(rp);
    splitter->setSizes({600, 500});

    root->addWidget(bar);
    root->addWidget(splitter, 1);
    return w;
}

// ═════════════════════════════════════════════════════════════════════════════
//  WebSocket Tab
// ═════════════════════════════════════════════════════════════════════════════
QWidget *DevToolsPanel::buildWsTab() {
    auto *w = new QWidget; w->setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(w);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    auto *bar = new QWidget; bar->setFixedHeight(34);
    bar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *bl = new QHBoxLayout(bar);
    bl->setContentsMargins(8,3,8,3); bl->setSpacing(8);
    m_wsCount = new QLabel("FRAMES: 0", bar); m_wsCount->setObjectName("cnt");

    auto *testBtn = btn("TEST WS (echo)", "#ffaa00", bar);
    testBtn->setToolTip("Opens wss://echo.websocket.org");
    connect(testBtn, &QPushButton::clicked, this, [this](){
        QMessageBox::information(this, "WS Test",
            "In the Browser tab, navigate to:\n\n"
            "  https://www.piesocket.com/websocket-tester\n\n"
            "That page opens a WebSocket to a demo server and sends/receives frames.\n"
            "You will see them appear in this WS tab immediately.\n\n"
            "Other good test sites:\n"
            "  https://socketsbay.com/test-websockets\n"
            "  https://websocket.org/tools/websocket-echo/");
    });

    bl->addWidget(m_wsCount); bl->addWidget(testBtn); bl->addStretch();

    auto *splitter = new QSplitter(Qt::Horizontal, w);

    m_wsTable = new QTableWidget(0, 4, splitter);
    m_wsTable->setHorizontalHeaderLabels({"DIR","SIZE","TIME","PREVIEW"});
    m_wsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_wsTable->setColumnWidth(0,70); m_wsTable->setColumnWidth(1,50); m_wsTable->setColumnWidth(2,72);
    m_wsTable->verticalHeader()->setVisible(false);
    m_wsTable->setShowGrid(false);
    m_wsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_wsTable, &QTableWidget::cellClicked, this, &DevToolsPanel::onWsRowSelected);

    auto *rp = new QWidget(splitter); rp->setStyleSheet(panelStyle());
    auto *rl = new QVBoxLayout(rp); rl->setContentsMargins(0,0,0,0); rl->setSpacing(0);

    auto *copyBar = new QWidget; copyBar->setFixedHeight(32);
    copyBar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *cbl = new QHBoxLayout(copyBar); cbl->setContentsMargins(6,2,6,2); cbl->setSpacing(4);
    auto *bCp = btn("COPY FRAME", "#888888", copyBar);
    auto *bDl = btn("DOWNLOAD",   "#ffaa00", copyBar);

    connect(bCp, &QPushButton::clicked, this,
            [this](){ clip(m_wsDetail->toPlainText()); });
    connect(bDl, &QPushButton::clicked, this, [this](){
        QString path = QFileDialog::getSaveFileName(
            this, "Save WS Frame", QDir::homePath() + "/ws_frame.txt",
            "Text Files (*.txt);;JSON (*.json);;All Files (*)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) return;
        QTextStream(&f) << m_wsDetail->toPlainText();
    });

    cbl->addWidget(bCp); cbl->addWidget(bDl); cbl->addStretch();

    m_wsDetail = new QTextEdit(rp);
    m_wsDetail->setReadOnly(true);
    m_wsDetail->setPlaceholderText("// select a frame\n\n// Tip: go to piesocket.com/websocket-tester in the Browser tab to see live WS frames");

    rl->addWidget(copyBar); rl->addWidget(m_wsDetail, 1);
    splitter->addWidget(m_wsTable); splitter->addWidget(rp);
    splitter->setSizes({500,600});

    root->addWidget(bar); root->addWidget(splitter,1);
    return w;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Cookies Tab
// ═════════════════════════════════════════════════════════════════════════════
QWidget *DevToolsPanel::buildCookiesTab() {
    auto *w = new QWidget; w->setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(w);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    auto *bar = new QWidget; bar->setFixedHeight(34);
    bar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *bl = new QHBoxLayout(bar); bl->setContentsMargins(8,3,8,3); bl->setSpacing(6);
    m_cookieCount = new QLabel("COOKIES: 0", bar); m_cookieCount->setObjectName("cnt");
    auto *copyAllBtn = btn("COPY ALL JSON", "#0088ff", bar);
    connect(copyAllBtn, &QPushButton::clicked, this, [this](){
        QString out = "{\n";
        for (auto &e : m_cookieEntries)
            out += QString("  \"%1\": \"%2\",\n").arg(e.cookie.name).arg(e.cookie.value);
        out += "}";
        clip(out);
    });
    bl->addWidget(m_cookieCount); bl->addStretch(); bl->addWidget(copyAllBtn);

    auto *splitter = new QSplitter(Qt::Vertical, w);

    m_cookieTable = new QTableWidget(0, 7, splitter);
    m_cookieTable->setHorizontalHeaderLabels({"NAME","VALUE","DOMAIN","PATH","HTTPONLY","SECURE","EXPIRES"});
    m_cookieTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_cookieTable->setColumnWidth(0,140); m_cookieTable->setColumnWidth(2,160);
    m_cookieTable->setColumnWidth(3,55);  m_cookieTable->setColumnWidth(4,68);
    m_cookieTable->setColumnWidth(5,55);  m_cookieTable->setColumnWidth(6,155);
    m_cookieTable->verticalHeader()->setVisible(false);
    m_cookieTable->setShowGrid(false);
    m_cookieTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cookieTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_cookieTable, &QTableWidget::cellClicked, this, &DevToolsPanel::onCookieRowSelected);

    auto *dp = new QWidget(splitter); dp->setStyleSheet(panelStyle());
    auto *dl = new QVBoxLayout(dp); dl->setContentsMargins(0,0,0,0); dl->setSpacing(0);

    auto *copyBar = new QWidget; copyBar->setFixedHeight(32);
    copyBar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *cbl = new QHBoxLayout(copyBar); cbl->setContentsMargins(6,2,6,2); cbl->setSpacing(4);
    auto *bAttr = btn("COPY COOKIE",  "#888888", copyBar);
    auto *bReq  = btn("COPY REQUEST", "#00cc66", copyBar);
    auto *bDl   = btn("DOWNLOAD",     "#ffaa00", copyBar);
    connect(bAttr, &QPushButton::clicked, this, [this](){ clip(m_cookieAttrView->toPlainText()); });
    connect(bReq,  &QPushButton::clicked, this, [this](){ clip(m_cookieRequestView->toPlainText()); });
    connect(bDl, &QPushButton::clicked, this, [this](){
        int row = m_cookieTable->currentRow();
        if (row < 0 || row >= m_cookieEntries.size()) return;
        QString name = m_cookieEntries[row].cookie.name;
        QString path = QFileDialog::getSaveFileName(
            this, "Save Cookie", QDir::homePath()+"/"+name+".txt",
            "Text Files (*.txt);;All Files (*)");
        if (path.isEmpty()) return;
        QFile f(path); if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) return;
        QTextStream(&f) << m_cookieAttrView->toPlainText()
                        << "\n\n" << m_cookieRequestView->toPlainText();
    });
    cbl->addWidget(bAttr); cbl->addWidget(bReq); cbl->addWidget(bDl); cbl->addStretch();

    m_cookieDetailTabs = new QTabWidget(dp);
    m_cookieDetailTabs->setStyleSheet(
        "QTabBar::tab { padding:4px 10px; font-size:10px; background:#090909; color:#444; "
        "border:none; border-right:1px solid #1a1a1a; }"
        "QTabBar::tab:selected { color:#00cc66; border-bottom:1px solid #00cc66; background:#0d0d0d; }");
    m_cookieAttrView    = new QTextEdit; m_cookieAttrView->setReadOnly(true);
    m_cookieAttrView->setPlaceholderText("// click a cookie above");
    m_cookieRequestView = new QTextEdit; m_cookieRequestView->setReadOnly(true);
    m_cookieRequestView->setPlaceholderText("// request that set this cookie");
    m_cookieDetailTabs->addTab(m_cookieAttrView,    "Cookie Info");
    m_cookieDetailTabs->addTab(m_cookieRequestView, "Set-By Request");

    dl->addWidget(copyBar); dl->addWidget(m_cookieDetailTabs, 1);
    splitter->addWidget(m_cookieTable); splitter->addWidget(dp);
    splitter->setSizes({350, 250});

    root->addWidget(bar); root->addWidget(splitter, 1);
    return w;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Storage Tab
// ═════════════════════════════════════════════════════════════════════════════
QWidget *DevToolsPanel::buildStorageTab() {
    auto *w = new QWidget; w->setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(w);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    auto *bar = new QWidget; bar->setFixedHeight(34);
    bar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *bl = new QHBoxLayout(bar); bl->setContentsMargins(8,3,8,3);
    m_storageCount = new QLabel("ENTRIES: 0", bar); m_storageCount->setObjectName("cnt");
    bl->addWidget(m_storageCount); bl->addStretch();

    auto *splitter = new QSplitter(Qt::Horizontal, w);

    m_storageTable = new QTableWidget(0, 4, splitter);
    m_storageTable->setHorizontalHeaderLabels({"TYPE","ORIGIN","KEY","VALUE"});
    m_storageTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_storageTable->setColumnWidth(0,100); m_storageTable->setColumnWidth(1,200);
    m_storageTable->setColumnWidth(2,160);
    m_storageTable->verticalHeader()->setVisible(false);
    m_storageTable->setShowGrid(false);
    m_storageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_storageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_storageTable, &QTableWidget::cellClicked, this, &DevToolsPanel::onStorageRowSelected);

    auto *rp = new QWidget(splitter); rp->setStyleSheet(panelStyle());
    auto *rl = new QVBoxLayout(rp); rl->setContentsMargins(0,0,0,0); rl->setSpacing(0);

    auto *copyBar = new QWidget; copyBar->setFixedHeight(32);
    copyBar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *cbl = new QHBoxLayout(copyBar); cbl->setContentsMargins(6,2,6,2); cbl->setSpacing(4);
    auto *bCp = btn("COPY VALUE", "#888888", copyBar);
    auto *bDl = btn("DOWNLOAD",   "#ffaa00", copyBar);
    connect(bCp, &QPushButton::clicked, this, [this](){ clip(m_storageDetail->toPlainText()); });
    connect(bDl, &QPushButton::clicked, this, [this](){
        QString path = QFileDialog::getSaveFileName(
            this, "Save Storage Entry", QDir::homePath()+"/storage_entry.json",
            "JSON Files (*.json);;Text Files (*.txt);;All Files (*)");
        if (path.isEmpty()) return;
        QFile f(path); if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) return;
        QTextStream(&f) << m_storageDetail->toPlainText();
    });
    cbl->addWidget(bCp); cbl->addWidget(bDl); cbl->addStretch();

    m_storageDetail = new QTextEdit(rp); m_storageDetail->setReadOnly(true);
    m_storageDetail->setPlaceholderText("// select an entry");

    rl->addWidget(copyBar); rl->addWidget(m_storageDetail,1);
    splitter->addWidget(m_storageTable); splitter->addWidget(rp);
    splitter->setSizes({600,500});

    root->addWidget(bar); root->addWidget(splitter,1);
    return w;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Export Tab
// ═════════════════════════════════════════════════════════════════════════════
QWidget *DevToolsPanel::buildExportTab() {
    auto *w = new QWidget; w->setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(w);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    auto *bar = new QWidget; bar->setFixedHeight(34);
    bar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *bl = new QHBoxLayout(bar); bl->setContentsMargins(8,3,8,3); bl->setSpacing(6);
    m_exportFormat = new QComboBox(bar);
    m_exportFormat->addItems({"Python (requests)","Python (curl_cffi)",
                               "cURL","JavaScript (fetch)","Raw HTTP"});
    m_exportFormat->setFixedWidth(160);
    auto *genBtn  = btn("GENERATE", "#00cc66", bar);
    auto *copyBtn = btn("COPY",     "#0088ff", bar); copyBtn->setFixedWidth(60);
    auto *dlBtn   = btn("DOWNLOAD", "#ffaa00", bar); dlBtn->setFixedWidth(90);
    connect(genBtn,  &QPushButton::clicked, this, &DevToolsPanel::exportSelected);
    connect(copyBtn, &QPushButton::clicked, this, [this](){ clip(m_exportArea->toPlainText()); });
    connect(dlBtn,   &QPushButton::clicked, this, [this](){
        QString path = QFileDialog::getSaveFileName(
            this, "Save Export", QDir::homePath()+"/request_export.py",
            "Python (*.py);;Shell (*.sh);;JavaScript (*.js);;Text (*.txt);;All (*)");
        if (path.isEmpty()) return;
        QFile f(path); if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) return;
        QTextStream(&f) << m_exportArea->toPlainText();
    });
    bl->addWidget(m_exportFormat); bl->addWidget(genBtn);
    bl->addWidget(copyBtn); bl->addWidget(dlBtn); bl->addStretch();

    m_exportArea = new QTextEdit(w); m_exportArea->setReadOnly(true);
    m_exportArea->setPlaceholderText("# select a request in Network tab, then click GENERATE");
    root->addWidget(bar); root->addWidget(m_exportArea,1);
    return w;
}