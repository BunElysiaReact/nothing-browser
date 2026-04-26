#include "WelcomeScreen.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

// ═════════════════════════════════════════════════════════════════════════════
//  WelcomeScreen  (T&C splash)
// ═════════════════════════════════════════════════════════════════════════════
WelcomeScreen::WelcomeScreen(QWidget *parent) : QWidget(parent) {
    setStyleSheet("background:#080808;");
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        "QScrollArea { border:none; background:#080808; }"
        "QScrollBar:vertical { background:#0a0a0a; width:6px; border:none; }"
        "QScrollBar::handle:vertical { background:#1e1e1e; border-radius:3px; min-height:20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
    );

    auto *content = new QWidget;
    content->setStyleSheet("background:#080808;");
    auto *cl = new QVBoxLayout(content);
    cl->setContentsMargins(80, 60, 80, 40);
    cl->setSpacing(0);

    auto makeLabel = [&](const QString &text, int size, const QString &color,
                         bool bold = false) {
        auto *l = new QLabel(text, content);
        l->setWordWrap(true);
        l->setStyleSheet(QString(
            "color:%1; font-family:monospace; font-size:%2px; %3 background:transparent;")
            .arg(color).arg(size).arg(bold ? "font-weight:bold;" : ""));
        return l;
    };

    auto makeSep = [&]() {
        auto *s = new QFrame(content);
        s->setFrameShape(QFrame::HLine);
        s->setStyleSheet("border:none; background:#1a1a1a; max-height:1px; margin:20px 0;");
        return s;
    };

    auto *logo = new QLabel(content);
    logo->setText(R"(
<pre style="color:#00cc66; font-family:monospace; font-size:13px; background:transparent;">
 ███╗   ██╗ ██████╗ ████████╗██╗  ██╗██╗███╗   ██╗ ██████╗
 ████╗  ██║██╔═══██╗╚══██╔══╝██║  ██║██║████╗  ██║██╔════╝
 ██╔██╗ ██║██║   ██║   ██║   ███████║██║██╔██╗ ██║██║  ███╗
 ██║╚██╗██║██║   ██║   ██║   ██╔══██║██║██║╚██╗██║██║   ██║
 ██║ ╚████║╚██████╔╝   ██║   ██║  ██║██║██║ ╚████║╚██████╔╝
 ╚═╝  ╚═══╝ ╚═════╝    ╚═╝   ╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝ ╚═════╝
                    B R O W S E R  v0.1
</pre>)");
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet("background:transparent;");

    cl->addWidget(logo);
    cl->addSpacing(10);
    cl->addWidget(makeLabel("Does nothing... except everything that matters.",
                            14, "#00cc66", true));
    cl->addSpacing(6);
    cl->addWidget(makeLabel(
        "Before you proceed, read this. It is short, honest, and important.",
        12, "#666"));
    cl->addWidget(makeSep());

    cl->addWidget(makeLabel("WHAT NOTHING BROWSER IS", 13, "#00cc66", true));
    cl->addSpacing(10);

    QStringList advantages = {
        "⬡  Full network inspector built in — see every request, response, WebSocket frame, cookie, and storage write as it happens.",
        "⬡  One-click export to Python, cURL, JavaScript or raw HTTP — reverse any API in minutes.",
        "⬡  Fingerprint spoofing at engine level — randomised Chrome UA, hardware concurrency, device memory, screen resolution, WebGL vendor.",
        "⬡  Zero telemetry. Zero analytics. Zero phoning home. We literally capture nothing about you.",
        "⬡  No history saved. No bookmarks synced. No passwords stored. Every session is disposable.",
        "⬡  Designed for scrapers, automation engineers, API researchers, and people who hate black boxes.",
        "⬡  70 MB base RAM target — run many instances without your machine crying.",
        "⬡  Downloadable captures — save any request, response, or WebSocket session to a file.",
    };
    for (auto &a : advantages) { cl->addSpacing(6); cl->addWidget(makeLabel(a, 12, "#aaaaaa")); }

    cl->addWidget(makeSep());
    cl->addWidget(makeLabel("WHAT NOTHING BROWSER IS NOT", 13, "#ff4444", true));
    cl->addSpacing(10);

    QStringList sideEffects = {
        "✕  Not a daily driver. Banking sites, Google properties, and Facebook will block or degrade you. This is expected.",
        "✕  Not a Chrome extension host. Extensions will not load.",
        "✕  Not anonymous. Fingerprint spoofing reduces entropy — it does not make you invisible. Use a VPN separately.",
        "✕  Not a captcha silver bullet in v0.1. The captcha solver is planned for v0.3.",
        "✕  Not a password manager. Store nothing sensitive in this browser.",
        "✕  Not production-hardened yet. You will find bugs. Report them.",
    };
    for (auto &s : sideEffects) { cl->addSpacing(6); cl->addWidget(makeLabel(s, 12, "#888888")); }

    cl->addWidget(makeSep());
    cl->addWidget(makeLabel("COMING IN LATER VERSIONS", 13, "#0088ff", true));
    cl->addSpacing(10);

    QStringList roadmap = {
        "→  v0.2 — Windows support, improved WS frame display, response body search",
        "→  v0.3 — Built-in captcha solver (reCAPTCHA v2/v3, hCaptcha)",
        "→  v0.4 — Script marketplace integration, headless mode",
        "→  v0.5 — Multi-tab support, session profiles, proxy manager",
        "→  v1.0 — Stable release, open-source UI layer, enterprise API",
    };
    for (auto &r : roadmap) { cl->addSpacing(6); cl->addWidget(makeLabel(r, 12, "#555555")); }

    cl->addWidget(makeSep());
    cl->addWidget(makeLabel("BY USING THIS SOFTWARE YOU UNDERSTAND:", 12, "#444", true));
    cl->addSpacing(8);
    cl->addWidget(makeLabel(
        "This tool is provided for legitimate network research, API reverse engineering, "
        "web scraping for lawful purposes, and personal automation. You are responsible "
        "for how you use it. The authors accept no liability for misuse. Using this tool "
        "to violate a website's terms of service is your decision and your consequence.",
        11, "#333"));

    cl->addSpacing(40);

    auto *scrollHint = new QLabel("↓  Scroll to the bottom to continue  ↓", content);
    scrollHint->setAlignment(Qt::AlignCenter);
    scrollHint->setStyleSheet(
        "color:#333; font-family:monospace; font-size:11px; background:transparent;");
    cl->addWidget(scrollHint);
    cl->addSpacing(20);

    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    // ── Bottom bar ────────────────────────────────────────────────────────────
    auto *bottomBar = new QWidget(this);
    bottomBar->setFixedHeight(54);
    bottomBar->setStyleSheet("background:#0a0a0a; border-top:1px solid #1a1a1a;");
    auto *bl = new QHBoxLayout(bottomBar);
    bl->setContentsMargins(40, 8, 40, 8);

    auto *hint = new QLabel("Scroll through the full list above, then click to proceed.", bottomBar);
    hint->setStyleSheet("color:#333; font-family:monospace; font-size:11px;");

    auto *acceptBtn = new QPushButton("I UNDERSTAND — OPEN NOTHING BROWSER", bottomBar);
    acceptBtn->setFixedHeight(36);
    acceptBtn->setEnabled(false);

    // Base style stored separately so disabled style can be appended cleanly
    const QString btnStyle = R"(
        QPushButton {
            background:#001a00; color:#00cc66;
            border:1px solid #00cc66; border-radius:3px;
            font-family:monospace; font-size:12px; font-weight:bold;
            padding:0 24px;
        }
        QPushButton:hover    { background:#002a00; }
        QPushButton:pressed  { background:#003300; }
        QPushButton:disabled { color:#1a3a1a; border-color:#1a3a1a; background:#050505; }
    )";
    acceptBtn->setStyleSheet(btnStyle);

    connect(acceptBtn, &QPushButton::clicked, this, &WelcomeScreen::accepted);

    // Enable the button once the user has scrolled to (near) the bottom
    connect(scroll->verticalScrollBar(), &QScrollBar::valueChanged,          // ← typo fixed
            this, [scroll, acceptBtn](int val) {
        int max = scroll->verticalScrollBar()->maximum();
        if (max > 0 && val >= max - 40)
            acceptBtn->setEnabled(true);
    });

    bl->addWidget(hint, 1);
    bl->addWidget(acceptBtn);
    root->addWidget(bottomBar);
}