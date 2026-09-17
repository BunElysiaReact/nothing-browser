# Piggy Browser Automation Library

**Piggy** is a powerful browser automation library that provides a clean, chainable API for controlling headless and headful browsers through WebSocket communication with the Nothing Browser binary.

## 🚀 Quick Start

```javascript
const piggy = require('piggy');

(async () => {
  // Launch browser (auto-detects existing instance or spawns new one)
  await piggy.launch({ mode: 'headless' });
  
  // Create a site and navigate
  const site = await piggy.site('google', { url: 'https://google.com' });
  
  // Extract data
  const title = await site.pageTitle();
  console.log(`Page title: ${title}`);
  
  // Cleanup
  await piggy.close();
})();
```

## 📦 Installation

1. **Build the Nothing Browser binary:**
```bash
cd /workspace
mkdir -p build && cd build
cmake ..
make
```

2. **Install Node.js dependencies:**
```bash
npm install ws ernest-logger
```

3. **Use the library:**
```javascript
const piggy = require('./lib');
```

## 🎯 Architecture

### Split WebSocket Logic

The WebSocket server (port 2005) runs **ONLY** in:
- `nothing_headless` - Headless browser mode
- `nothing_headful` - Headful browser mode

The main GUI browser (`nothing`) does **NOT** start a WebSocket server.

### Connection Modes

1. **Launch (Local)** - Spawns a new binary or joins existing instance
2. **Connect (Remote)** - Connects to a remote/hosted instance

```javascript
// Launch local instance
await piggy.launch({ mode: 'headless' });

// Connect to remote instance
await piggy.connect({ host: '192.168.1.100', key: 'my-secret-key' });
```

## 📖 API Reference

### Main Piggy Object

#### `launch(options)`
Launch or join a browser instance.

```javascript
await piggy.launch({
  mode: 'headless',      // 'headless' or 'headful'
  binary: '/custom/path', // Optional custom binary path
  args: [],              // Additional CLI arguments
  key: 'auth-key'        // Optional authentication key
});
```

#### `connect(options)`
Connect to a remote Piggy instance.

```javascript
await piggy.connect({
  host: '192.168.1.100', // Hostname or IP
  key: 'auth-key'        // Authentication key
});
```

#### `site(name, options)`
Create a new site/tab.

```javascript
const site = await piggy.site('mysite', {
  url: 'https://example.com' // Optional initial URL
});
```

#### `actHuman(enable)`
Enable/disable human-like behavior patterns.

```javascript
piggy.actHuman(true);  // Enable human mode
piggy.actHuman(false); // Disable
```

#### `expose(name, handler, tabId)`
Expose a JavaScript function to the browser context.

```javascript
await piggy.expose('myFunction', (data) => {
  console.log('Called from browser:', data);
  return 'response';
});
```

#### `close(options)`
Close connection and optionally shutdown binary.

```javascript
await piggy.close({ 
  force: true,     // Force close everything
  shutdown: true   // Shutdown the binary
});
```

### Site Object (Chainable API)

All Site methods return `Promise<Site>` for chaining, except data extraction methods.

#### Navigation

```javascript
await site.goto('https://example.com');
await site.refresh();
await site.back();
await site.forward();
await site.waitNavigation();
await site.waitSelector('.my-element');
await site.waitResponse('/api/data');
```

#### Interactions

```javascript
await site.click('#submit-btn');
await site.dblclick('.item');
await site.hover('.dropdown');
await site.type('#email', 'test@example.com');
await site.select('#country', 'US');
await site.upload('#file-input', '/path/to/file.pdf');
await site.scrollTo('.footer');
await site.scrollBy(0, 500);
await site.drag('.draggable', '.droppable');
await site.mouseMove(100, 200);
await site.press('Enter');
await site.combo(['Control', 'C']);
```

#### Dialogs

```javascript
await site.acceptDialog('prompt text');
await site.dismissDialog();
await site.setDialogAutoAction('accept'); // or 'dismiss'
await site.waitAndAcceptDialog();
await site.waitAndDismissDialog();
const status = await site.getDialogStatus();
```

#### Element Queries (Find)

```javascript
const exists = await site.exists('#my-id');
const visible = await site.visible('.my-class');
const hasClass = await site.hasClass('div', 'active');
const hasAttr = await site.hasAttr('a', 'href');
const hasText = await site.hasText('h1', 'Welcome');
const matches = await site.matches('button.primary');
const checked = await site.checked('#agree-checkbox');
const enabled = await site.enabled('#submit');
```

#### Data Extraction (Provide)

```javascript
const text = await site.text('h1');
const allText = await site.textAll('p');
const html = await site.html('.content');
const attr = await site.attr('img', 'src');
const allAttrs = await site.attrAll('a', 'href');
const links = await site.links();
const images = await site.images();
const formData = await site.form('#login-form');
const tableData = await site.table('#data-table');
const listItems = await site.list('ul.items');
const metaTags = await site.meta();
const jsonData = await site.json();
const content = await site.pageContent();
const title = await site.pageTitle();
const url = await site.pageUrl();

// Advanced selectors
const byAttr = await site.byAttr('data-id', '123');
const byPlaceholder = await site.byPlaceholder('Search...');
const byRole = await site.byRole('button');
const byTag = await site.byTag('article');
const children = await site.children('.parent');
const closest = await site.closest('.child', '.ancestor');
const parent = await site.parent('.child');
const count = await site.count('.items');
const first = await site.first('.item');
const filtered = await site.filter('.item', { visible: true });
const selected = await site.selectElements('.selected');
const divContent = await site.div('.main');
```

#### JavaScript Evaluation

```javascript
const result = await site.evaluate(`
  document.querySelector('h1').innerText
`);
```

#### Screenshots & PDF

```javascript
await site.screenshot('./page.png', {
  fullPage: true,
  clip: { x: 0, y: 0, width: 800, height: 600 }
});

await site.pdf('./page.pdf', {
  landscape: false,
  printBackground: true,
  paperWidth: 8.5,
  paperHeight: 11
});

await site.exportJson('./data.json', { key: 'value' });
```

#### Network Capture

```javascript
await site.startCapture();
// ... perform actions ...
const requests = await site.getRequests();
const wsFrames = await site.getWsFrames();
await site.stopCapture();
await site.clearCapture();
```

#### Request Interception

```javascript
await site.blockImages();
await site.unblockImages();
```

#### Media Control

```javascript
await site.playMedia('video');
await site.pauseMedia('video');
await site.muteMedia();
await site.unmuteMedia();
```

### Proxy Manager

```javascript
// Enable/disable
await piggy.proxy.enable();
await piggy.proxy.disable();

// Set/get proxy
await piggy.proxy.set('user:pass@proxy.example.com:8080');
const current = await piggy.proxy.current();

// Rotation
await piggy.proxy.rotate();
const nextProxy = await piggy.proxy.next();

// Load proxies
await piggy.proxy.load('./proxies.txt');
await piggy.proxy.fetch('https://proxy-source.com/list');

// Testing
const results = await piggy.proxy.test();
await piggy.proxy.stopTest();

// Stats and management
const stats = await piggy.proxy.stats();
const list = await piggy.proxy.list();
await piggy.proxy.save('./saved-proxies.txt');

// OpenVPN
await piggy.proxy.ovpn('./config.ovpn');

// Configuration
await piggy.proxy.rotation({ mode: 'round-robin', interval: 60 });
await piggy.proxy.config({ timeout: 5000, retries: 3 });
```

### Multi-Site Operations

```javascript
const site1 = await piggy.site('site1', { url: 'https://a.com' });
const site2 = await piggy.site('site2', { url: 'https://b.com' });

// Execute on all sites
await piggy.all([site1, site2]).refresh();

// Get differentiated results
const titles = await piggy.diff([site1, site2]).pageTitle();
console.log(titles.site1); // Title from site1
console.log(titles.site2); // Title from site2
```

### Events

```javascript
piggy.on('navigate', (data) => {
  console.log(`Navigated to ${data.url} on tab ${data.tabId}`);
});

piggy.on('captcha', (data) => {
  console.warn(`CAPTCHA detected: ${data.captchaType}`);
});

piggy.on('blocked', (data) => {
  console.warn(`Block detected: ${data.blockType}`);
});

piggy.on('dialog', (data) => {
  console.log(`Dialog on tab ${data.tabId}`);
});

// Proxy events
piggy.proxy.on('proxy:changed', (data) => {
  console.log(`New proxy: ${data.proxy} (${data.latency}ms)`);
});

piggy.proxy.on('proxy:dead', (data) => {
  console.warn(`Proxy dead: index ${data.index}`);
});

piggy.proxy.on('proxy:exhausted', () => {
  console.error('All proxies exhausted!');
});
```

## 🔧 Logger

The library includes a built-in logger with emoji support:

```javascript
const logger = require('piggy/logger');

logger.trace('Trace message');
logger.debug('Debug message');
logger.info('Info message');
logger.success('Success message');
logger.warn('Warning message');
logger.error('Error message');
logger.network('Network activity');
logger.db('Database operation');
logger.security('Security event');
```

Logs are written to `./piggy.log` by default.

## 🧪 Running Tests

```bash
# Basic tests
node lib/test/basic.test.js

# Advanced tests
node lib/test/advanced.test.js
```

## 📝 Examples

### Web Scraping

```javascript
const piggy = require('piggy');

(async () => {
  await piggy.launch({ mode: 'headless' });
  const site = await piggy.site('scraper');
  
  await site.goto('https://example-products.com');
  
  const products = await site.diff([
    { selector: '.product', method: 'textAll' }
  ]);
  
  const prices = await site.textAll('.price');
  const names = await site.textAll('.product-name');
  
  console.log({ names, prices });
  
  await piggy.close();
})();
```

### Form Automation

```javascript
const piggy = require('piggy');

(async () => {
  await piggy.launch({ mode: 'headless' });
  const site = await piggy.site('form-filler');
  
  await site.goto('https://example.com/signup');
  
  await site.type('#name', 'John Doe');
  await site.type('#email', 'john@example.com');
  await site.type('#password', 'securepass123');
  await site.select('#country', 'USA');
  await site.click('#terms-checkbox');
  await site.click('#submit-btn');
  
  await site.waitNavigation();
  const success = await site.exists('.success-message');
  
  console.log('Signup successful:', success);
  
  await piggy.close();
})();
```

### Multi-Tab Monitoring

```javascript
const piggy = require('piggy');

(async () => {
  await piggy.launch({ mode: 'headless' });
  
  const sites = await Promise.all([
    piggy.site('site1', { url: 'https://status1.com' }),
    piggy.site('site2', { url: 'https://status2.com' }),
    piggy.site('site3', { url: 'https://status3.com' })
  ]);
  
  // Check all sites every 30 seconds
  setInterval(async () => {
    const statuses = await piggy.diff(sites).pageTitle();
    console.log('Status check:', statuses);
  }, 30000);
  
  // Keep running
  process.stdin.resume();
})();
```

## ⚠️ Important Notes

1. **Port 2005** - Fixed WebSocket port for all connections
2. **Shared Daemon** - Multiple scripts can connect to the same binary instance
3. **Tab Ownership** - Tabs are owned by the client that created them
4. **Authentication** - Use `key` option for secure connections
5. **Binary Required** - Must build `nothing_headless` or `nothing_headful` before using

## 🐛 Troubleshooting

### "Binary not found"
```bash
cd /workspace/build
make
```

### "Connection refused"
Ensure the binary is running or use `launch()` to start it.

### "Authentication failed"
Check that you're using the correct `key` in both binary launch and client connect.

### "Command timed out"
Increase timeout: `site.timeout(60000)` for longer operations.

## 📄 License

MIT License - See LICENSE file for details.
