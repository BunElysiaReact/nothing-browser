# Nothing Browser

> **Does nothing... except everything that matters.**

A scrapper-first browser built on Qt6 + Chromium WebEngine. Designed for API reverse engineering, web scraping, automation research, and people who hate black boxes.

---

## What it is

Most browsers are built to hide the web from you. Nothing Browser is built to expose it.

Every request, response, WebSocket frame, cookie, and storage write is captured in real time — visible, inspectable, copyable, and downloadable. One click exports any captured request to Python, cURL, JavaScript, or raw HTTP.

```
┌─────────────────────────────────────────────────────────────┐
│  DEVTOOLS  │  BROWSER  │  YOUTUBE  │  TECH HOUSE           │
├─────────────────────────────────────────────────────────────┤
│  NETWORK [247]  │  WS [12]  │  COOKIES [38]  │  STORAGE   │
│                                                             │
│  POST  200  Fetch  1.2k  https://api.example.com/v1/data  │
│  GET   304  Script  -    https://cdn.example.com/app.js   │
│                                                             │
│  [COPY HEADERS] [COPY RESPONSE] [AS CURL] [AS PYTHON]     │
└─────────────────────────────────────────────────────────────┘
```

---

## Features

- **Full network inspector** — every HTTP request/response with Firefox-style header view
- **WebSocket capture** — every frame, direction-tagged, JSON auto pretty-printed
- **Cookie inspector** — see the exact request that set each cookie
- **Storage capture** — localStorage and sessionStorage writes as they happen
- **One-click export** — Python `requests`, `curl_cffi`, cURL, JavaScript fetch, raw HTTP
- **Download everything** — save any request, response, or WS session to a file
- **Fingerprint spoofing** — randomised Chrome UA, hardware concurrency, device memory, screen res, WebGL vendor at engine level
- **Zero telemetry** — no analytics, no phoning home, nothing stored after session ends
- **Update notifications** — checks for new versions on launch, bell icon + changelog

---

## Install

### Linux (Debian / Ubuntu / Arch — one line)

```bash
curl -fsSL https://raw.githubusercontent.com/BunElysiaReact/nothing-browser/main/install.sh | sudo bash
```

### Debian / Ubuntu (.deb)

```bash
# Download latest .deb from releases
sudo dpkg -i nothing-browser_0.1.0_amd64.deb
```

### Arch Linux (AUR)

```bash
yay -S nothing-browser
```

### macOS (Homebrew)

```bash
brew tap BunElysiaReact/nothing
brew install nothing-browser
```

### Windows

Download the zip from [Releases](https://github.com/BunElysiaReact/nothing-browser/releases), extract, run `nothing-browser.exe`. No install needed.

---

## Build from source

### Requirements

| Platform | Dependencies |
|----------|-------------|
| Linux    | Qt6 WebEngine, CMake 3.20+, GCC/Clang |
| macOS    | Qt6 (Homebrew), CMake, Xcode CLT |
| Windows  | Qt6 (Qt Installer), CMake, Visual Studio 2022 |

### Linux / macOS

```bash
git clone https://github.com/BunElysiaReact/nothing-browser.git
cd nothing-browser
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./nothing-browser
```

### Windows

```bat
# Open Developer Command Prompt
# Edit QT6_DIR in scripts/build-windows.bat first
scripts\build-windows.bat
```

---

## Project structure

```
nothing-browser/
├── core/
│   ├── app/                  # MainWindow
│   ├── engine/               # Interceptor, FingerprintSpoofer,
│   │                         # NetworkCapture, UpdateChecker
│   ├── tabs/                 # BrowserTab, DevToolsPanel,
│   │                         # NewsTab, YoutubeTab
│   └── main.cpp
├── assets/
│   └── icons/logo.svg
├── scripts/
│   ├── build-deb.sh          # Build .deb package
│   ├── build-windows.bat     # Build Windows release
│   ├── PKGBUILD              # Arch AUR package
│   ├── nothing-browser.rb    # Homebrew formula
│   └── release.sh            # Tag + publish full release
├── version.json              # Polled by app for update checks
├── install.sh                # Universal Linux installer
└── CMakeLists.txt
```

---

## How updates work

The app pings `version.json` at this repo on launch and every 6 hours. If a newer version is found, the notification bell lights up in the Tech House tab with the full changelog.

To release a new version:

```bash
bash scripts/release.sh 0.2.0 "Fixed WS capture, added Windows support"
```

---

## Roadmap

| Version | Status | What |
|---------|--------|------|
| v0.1.0  | ✅ Now  | Core browser + full DevTools scrapper panel |
| v0.2.0  | 🔨 Next | Windows support, response body search |
| v0.3.0  | Planned | Built-in captcha solver (reCAPTCHA / hCaptcha) |
| v0.4.0  | Planned | Script marketplace + headless mode |
| v0.5.0  | Planned | Multi-tab, session profiles, proxy manager |
| v1.0.0  | Planned | Stable release, open-source UI layer |

---

## Known limitations

- Google properties, Facebook, and banking sites **will block you** — this is expected
- Chrome extensions are not supported
- Fingerprint spoofing reduces entropy, it does not make you invisible
- Captcha solver coming in v0.3

---

## Built with

- [Qt6 WebEngine](https://doc.qt.io/qt-6/qtwebengine-index.html) — Chromium-based browser engine
- [Bun](https://bun.sh) — powers the SCRAPPER side project
- [Elysia.js](https://elysiajs.com) — API layer for the scrapper server

---

## License

MIT — use it, build on it, sell scripts with it. Just don't blame us when Facebook blocks you.

---

*Part of the [BunElysiaReact](https://github.com/BunElysiaReact) ecosystem.*