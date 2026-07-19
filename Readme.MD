
# Nothing Browser

> **Does nothing... except everything that matters.**

A scraper-first browser built on Qt6 + Chromium WebEngine. Designed for API reverse engineering, web scraping, automation research, and people who hate black boxes.

---

## 📚 Documentation

**[https://nothing-browser-docs.pages.dev/guide/nothing-browser/](https://nothing-browser-docs.pages.dev/guide/nothing-browser/)**

Full documentation including installation, DevTools, network capture, WebSocket inspector, and plugin system.

---

## What it is

Most browsers are built to hide the web from you. Nothing Browser is built to **expose it**.

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
- **Auto-update** — checks GitHub Releases on launch and every 6 hours, downloads and installs updates in-app with one click
- **Plugin system** — community JS plugins installed from inside the browser
- **YouTube tab** — YouTube client without API keys (via NewPipe Extractor)

---

## Install

### Linux (Debian / Ubuntu)

```bash
# Install dependencies first
sudo apt-get install -f

# Then install the .deb
sudo dpkg -i nothing-browser_*_amd64.deb
```

Or with apt (handles dependencies automatically):
```bash
sudo apt install ./nothing-browser_*_amd64.deb
```

### Linux (tar.gz — no dependencies needed if Qt6 is installed)

```bash
tar -xzf nothing-browser-*-linux-x86_64.tar.gz
cd nothing-browser-*-linux-x86_64
./nothing-browser
```

### Arch Linux (AUR)

```bash
yay -S nothing-browser
```

### macOS

Download the `.dmg` from [Releases](https://github.com/BunElysiaReact/nothing-browser/releases) → drag to Applications. JRE bundled, no install needed.

### Windows

Download the `.zip` from [Releases](https://github.com/BunElysiaReact/nothing-browser/releases) → extract → run `nothing-browser.exe`. JRE bundled, no install needed.

---

## Quick Start (Piggy Library)

The `nothing-browser` npm package lets you control the browser from TypeScript/Bun:

```ts
import piggy from "nothing-browser";

await piggy.launch({ mode: "tab" });
await piggy.register("books", "https://books.toscrape.com");

await piggy.books.navigate();
const books = await piggy.books.evaluate(() =>
  Array.from(document.querySelectorAll(".product_pod")).map(el => ({
    title: el.querySelector("h3 a")?.getAttribute("title") ?? "",
    price: el.querySelector(".price_color")?.textContent?.trim() ?? "",
  }))
);

console.log(books);
await piggy.close();
```

> **Full Piggy documentation:** [nothing-browser-docs.pages.dev/guide/piggy/quickstart](https://nothing-browser-docs.pages.dev/guide/piggy/quickstart)

---

## Build from source

### Requirements

| Platform | Dependencies |
|----------|-------------|
| Linux    | Qt6 WebEngine, CMake 3.20+, GCC/Clang, Java 17 |
| macOS    | Qt6 (Homebrew), CMake, Xcode CLT, Java 17 |
| Windows  | Qt6 (Qt Installer), CMake, Visual Studio 2022, Java 17 |

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
│   │                         # NewsTab, YoutubeTab, PluginsTab
│   └── main.cpp
├── assets/
│   └── icons/logo.svg
├── scripts/
│   ├── build-deb.sh          # Build .deb package
│   ├── build-windows.bat     # Build Windows release
│   ├── PKGBUILD              # Arch AUR package
│   └── nothing-browser.rb    # Homebrew formula
├── .github/
│   └── workflows/
│       └── build-release.yml # CI — builds all platforms on tag push
├── install.sh                # Universal Linux installer
└── CMakeLists.txt
```

---

## How updates work

The app checks the [GitHub Releases API](https://api.github.com/repos/BunElysiaReact/nothing-browser/releases/latest) on launch and every 6 hours. If a newer version exists the notification bell lights up in the Tech House tab showing the full changelog.

Clicking **DOWNLOAD UPDATE** streams the binary directly into a temp folder with a live progress bar. Clicking **INSTALL & RESTART** writes an updater script, closes the app, swaps the binary, and restarts — no terminal needed.

To ship a new release:

```bash
git add .
git commit -m "feat: what changed"
git push
git tag v0.1.9
git push origin v0.1.9
```

GitHub Actions builds Linux, Windows, and macOS automatically and publishes the release with all assets attached.

---

## Roadmap

| Version | Status | What |
|---------|--------|------|
| v0.1.0  | ✅ Done  | Core browser + full DevTools scrapper panel |
| v0.1.1  | ✅ Done  | Auto-update system, YouTube loop, UI improvements |
| v0.1.2  | ✅ Done  | Plugin system, community registry |
| v0.1.3  | ✅ Done  | Fingerprint improvements, xorshift canvas noise |
| v0.1.9  | ✅ Latest | Windows support, bug fixes |
| v0.2.0  | 🔨 Next  | Response body search, identity reset UI |
| v0.3.0  | 📋 Planned | Built-in captcha solver (reCAPTCHA / hCaptcha) |
| v0.4.0  | 📋 Planned | Script marketplace + headless mode |
| v0.5.0  | 📋 Planned | Multi-tab, session profiles, proxy manager |
| v1.0.0  | 📋 Planned | Stable release, open-source UI layer |

---

## Known limitations

- Google properties, Facebook, and banking sites **will block you** — this is expected
- Chrome extensions are not supported
- Fingerprint spoofing reduces entropy, it does not make you invisible
- Captcha solver coming in v0.3
- Auto-update install requires the app binary to be writable (not installed to `/usr/bin` via deb — use the tar.gz for auto-update to work)

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and the [Contributing Guide](https://nothing-browser-docs.pages.dev/guide/community/contributing).

**Quick rules:**
- ❌ Don't touch `fingerprintspoofer.cpp`, `identitygenerator.cpp`, or `interceptor.cpp`
- ❌ Don't add settings to Private Browser
- ✅ UI changes (curved tabs, icons, fonts) are welcome
- ✅ Bug fixes are welcome

---

## Security

Report security vulnerabilities to **ernesttechhouse@gmail.com** — do NOT open a public issue.

See [SECURITY.md](SECURITY.md) for details.

---

## Built with

- [Qt6 WebEngine](https://doc.qt.io/qt-6/qtwebengine-index.html) — Chromium-based browser engine
- [NewPipe Extractor](https://github.com/TeamNewPipe/NewPipeExtractor) — YouTube data without the API
- [Bun](https://bun.sh) — powers the Piggy library
- [Elysia.js](https://elysiajs.com) — API layer for the scraper server

---

## License

MIT — use it, build on it, sell scripts with it. Just don't blame us when Facebook blocks you.

---

## Links

- 📚 **Documentation:** [nothing-browser-docs.pages.dev](https://nothing-browser-docs.pages.dev)
- 📦 **npm package:** [nothing-browser](https://www.npmjs.com/package/nothing-browser)
- 🐙 **GitHub:** [BunElysiaReact/nothing-browser](https://github.com/BunElysiaReact/nothing-browser)
- 💬 **Discord:** [Join server](https://discord.gg/TUxBVQ7y)
- 📱 **WhatsApp Channel:** [Ernest Tech House](https://whatsapp.com/channel/0029VbBzoXuCxoArtvaslR0U)

---

*Part of the [Nothing Ecosystem](https://nothing-browser-docs.pages.dev). Built in Kenya 🇰🇪*
