'use strict';

const { EventEmitter } = require('events');
const { PiggyClient }  = require('./client');
const { resolveBinary, spawnBinary, detectBinary } = require('./launcher');
const { createSite }   = require('./site');
const logger = require('./logger');

/**
 * Piggy - Main browser automation library
 * Provides a chainable API for controlling headless/headful browsers
 */
class Piggy extends EventEmitter {
  constructor() {
    super();
    this._client    = null;
    this._proc      = null;
    this._sites     = {};
    this._plugins   = [];
    this._humanMode = false;
    this._tabMode   = 'tab';
    this._proxy     = new ProxyManager(this);
  }

  /**
   * Get proxy manager
   * @returns {ProxyManager}
   */
  get proxy() {
    return this._proxy;
  }

  // ── Launch (local) ─────────────────────────────────────────────────────────
  //
  // First checks whether a Piggy instance is already listening on the fixed
  // WebSocket port (2005). If so, this script just joins it — no new binary,
  // no new browser process, just another connection sharing the same daemon.
  // Only if nothing answers does it spawn a fresh binary and wait for it.

  async launch(opts = {}) {
    const { mode = 'headless', binary, args = [], key } = opts;
    this._tabMode = mode;

    const probeClient = new PiggyClient({ key });
    let alreadyRunning = false;
    try {
      alreadyRunning = await probeClient.probe();
    } catch (err) {
      if (err.authFailure) {
        // Something IS listening on 2005 — it just rejected our key.
        // Spawning a second binary would only fail to bind the port and
        // confuse things further, so surface the real problem instead.
        logger.error('An instance is already running on port 2005, but it rejected this key.');
        throw err;
      }
      // Otherwise treat it like "nothing there" and fall through to spawn.
    }

    if (alreadyRunning) {
      logger.info('Existing Piggy instance found on port 2005 — joining it');
      this._client = probeClient;
    } else {
      logger.info(`Launching Nothing Browser (mode: ${mode})`);
      const binPath = resolveBinary(binary, mode);
      this._proc = await spawnBinary(binPath, { args });
      this._client = new PiggyClient({ key });
      await this._client.connect(); // retries internally until the WS server is up
    }

    this._wireGlobalEvents();
    logger.success('Piggy ready');
    return this;
  }

  // ── Connect (remote, or a specific known instance) ────────────────────────
  // Same WebSocket transport as launch() — just pointed at a host, and with
  // a key if that instance requires one. Port is still always 2005.

  async connect(opts = {}) {
    // Lenient: accept a bare hostname ("1.2.3.4") or an old-style URL
    // ("http://1.2.3.4:2005") — either way we only need the hostname, since
    // the port is always 2005 now.
    let host = opts.host || '127.0.0.1';
    if (/^https?:\/\//i.test(host)) host = new URL(host).hostname;
    logger.info(`Connecting to Piggy at ${host}:2005`);
    this._client = new PiggyClient({ host, key: opts.key });
    await this._client.connect();
    this._wireGlobalEvents();
    logger.success(`Connection established (${host}:2005)`);
    return this;
  }

  // ── Wire global events ────────────────────────────────────────────────────

  _wireGlobalEvents() {
    const c = this._client;

    const proxyEvents = [
      'proxy:changed', 'proxy:loaded', 'proxy:fetch:failed',
      'proxy:check:started', 'proxy:check:done',
      'proxy:alive', 'proxy:dead', 'proxy:exhausted', 'proxy:ovpn:loaded',
    ];

    proxyEvents.forEach(ev => c.on(ev, d => {
      this.emit(ev, d);
      this._proxy.emit(ev, d);

      // User-visible proxy messages
      if (ev === 'proxy:changed')      logger.network(`Proxy rotated → ${d.proxy} (${d.latency}ms)`);
      if (ev === 'proxy:exhausted')    logger.warn('All proxies exhausted');
      if (ev === 'proxy:fetch:failed') logger.error(`Proxy fetch failed: ${d.error}`);
      if (ev === 'proxy:dead')         logger.warn(`Proxy ${d.index} dead (${d.latency}ms)`);
      if (ev === 'proxy:alive')        logger.debug(`Proxy ${d.index} alive (${d.latency}ms)`);
      if (ev === 'proxy:check:done')   logger.info(`Proxy check done: ${d.alive} alive, ${d.dead} dead`);
    }));

    c.on('navigate', d => {
      this.emit('navigate', d);
      logger.debug(`Navigate → ${d.url} (tab: ${d.tabId})`);
    });

    c.on('captcha', d => {
      this.emit('captcha', d);
      logger.warn(`CAPTCHA detected (${d.captchaType}) on tab ${d.tabId}`);
    });

    c.on('captcha:resolved', d => {
      this.emit('captcha:resolved', d);
      logger.success(`CAPTCHA resolved on tab ${d.tabId}`);
    });

    c.on('blocked', d => {
      this.emit('blocked', d);
      logger.warn(`Block detected (${d.blockType}) on tab ${d.tabId}`);
    });

    c.on('dialog', d => {
      this.emit('dialog', d);
      logger.warn(`Dialog detected on tab ${d.tabId}`);
    });
  }

  // ── Site management ───────────────────────────────────────────────────────

  /**
   * Create a new site/tab
   * @param {string} name - Site name
   * @param {object} opts - Options
   * @returns {Promise<Site>}
   */
  async site(name, opts = {}) {
    const tabId = await this._client.send('tab.new', {});
    const site = createSite(this._client, name, tabId);
    this._sites[name] = site;
    
    if (opts.url) {
      await site.goto(opts.url);
    }
    
    logger.success(`Created site: ${name} (tab: ${tabId})`);
    return site;
  }

  /**
   * Get existing site by name
   * @param {string} name - Site name
   * @returns {Site|undefined}
   */
  getSite(name) {
    return this._sites[name];
  }

  /**
   * List all sites
   * @returns {string[]}
   */
  listSites() {
    return Object.keys(this._sites);
  }

  // ── Global controls ───────────────────────────────────────────────────────

  /**
   * Enable/disable human-like behavior
   * @param {boolean} enable
   * @returns {Piggy}
   */
  actHuman(enable) {
    this._humanMode = enable;
    logger.info(`Human mode: ${enable}`);
    return this;
  }

  /**
   * Set tab mode
   * @param {string} mode - Tab mode
   * @returns {Piggy}
   */
  mode(m) {
    this._tabMode = m;
    return this;
  }

  /**
   * Expose a function to the browser context
   * @param {string} name - Function name
   * @param {Function} handler - Handler function
   * @param {string} tabId - Tab ID (optional)
   * @returns {Promise<Piggy>}
   */
  async expose(name, handler, tabId = 'default') {
    // Serialize function and send to browser
    const fnStr = handler.toString();
    await this._client.send('expose', { name, fn: fnStr, tabId });
    logger.success(`Exposed function: ${name}`);
    return this;
  }

  /**
   * Remove exposed function
   * @param {string} name - Function name
   * @param {string} tabId - Tab ID (optional)
   * @returns {Promise<Piggy>}
   */
  async unexpose(name, tabId = 'default') {
    await this._client.send('unexpose', { name, tabId });
    logger.info(`Unexposed function: ${name}`);
    return this;
  }

  // ── Multi-site helpers ────────────────────────────────────────────────────

  /**
   * Execute method on all sites
   * @param {Site[]} sites - Array of sites
   * @returns {Proxy}
   */
  all(sites) {
    return new Proxy({}, {
      get: (_, method) =>
        (...args) => Promise.all(sites.map(s => s[method]?.(...args))),
    });
  }

  /**
   * Execute method on all sites and return differentiated results
   * @param {Site[]} sites - Array of sites
   * @returns {Proxy}
   */
  diff(sites) {
    return new Proxy({}, {
      get: (_, method) =>
        async (...args) => {
          const results = await Promise.all(sites.map(s => s[method]?.(...args)));
          return Object.fromEntries(sites.map((s, i) => [s._name ?? i, results[i]]));
        },
    });
  }

  // ── Shutdown ──────────────────────────────────────────────────────────────

  /**
   * Close Piggy connection and optionally shutdown binary
   * @param {object} opts - Options
   * @param {boolean} opts.force - Force close everything
   * @param {boolean} opts.shutdown - Shutdown the binary
   * @returns {Promise<void>}
   */
  async close(opts = {}) {
    if (opts.force || opts.shutdown) {
      // Close all sites
      for (const site of Object.values(this._sites)) {
        await site.close().catch(() => {});
      }
      
      if (opts.shutdown && this._client) {
        await this._client.send('shutdown', {});
      }
      
      if (this._client) {
        this._client.disconnect();
        this._client = null;
      }
      
      if (this._proc) {
        this._proc.kill();
        this._proc = null;
      }
      
      this._sites = {};
      logger.info('Piggy closed (force/shutdown)');
    } else {
      // Gentle close - just disconnect client
      for (const site of Object.values(this._sites)) {
        await site.close().catch(() => {});
      }
      
      if (this._client) {
        this._client.disconnect();
        this._client = null;
      }
      
      this._sites = {};
      logger.info('Piggy disconnected');
    }
  }
}

// ── Proxy Manager ───────────────────────────────────────────────────────────

class ProxyManager extends EventEmitter {
  constructor(piggy) {
    super();
    this._piggy = piggy;
  }

  async _send(cmd, payload) {
    return this._piggy._client.send(cmd, payload);
  }

  /**
   * Enable proxy
   * @returns {Promise<void>}
   */
  async enable() {
    await this._send('proxy.enable', {});
    logger.success('Proxy enabled');
    return this;
  }

  /**
   * Disable proxy
   * @returns {Promise<void>}
   */
  async disable() {
    await this._send('proxy.disable', {});
    logger.info('Proxy disabled');
    return this;
  }

  /**
   * Set proxy server
   * @param {string} proxy - Proxy address (host:port or user:pass@host:port)
   * @returns {Promise<void>}
   */
  async set(proxy) {
    await this._send('proxy.set', { proxy });
    logger.success(`Proxy set: ${proxy}`);
    return this;
  }

  /**
   * Get current proxy
   * @returns {Promise<string>}
   */
  async current() {
    const result = await this._send('proxy.current', {});
    return result;
  }

  /**
   * Rotate to next proxy
   * @returns {Promise<void>}
   */
  async rotate() {
    await this._send('proxy.rotate', {});
    logger.debug('Proxy rotated');
    return this;
  }

  /**
   * Get next proxy without rotating
   * @returns {Promise<string>}
   */
  async next() {
    const result = await this._send('proxy.next', {});
    return result;
  }

  /**
   * Load proxy list from file
   * @param {string} file - Path to proxy list file
   * @returns {Promise<void>}
   */
  async load(file) {
    await this._send('proxy.load', { file });
    logger.success(`Proxy list loaded: ${file}`);
    return this;
  }

  /**
   * Fetch proxies from URL
   * @param {string} url - URL to fetch proxies from
   * @returns {Promise<void>}
   */
  async fetch(url) {
    await this._send('proxy.fetch', { url });
    logger.success(`Proxies fetched from: ${url}`);
    return this;
  }

  /**
   * Test all proxies
   * @returns {Promise<object>}
   */
  async test() {
    const result = await this._send('proxy.test', {});
    logger.info(`Proxy test completed: ${result.alive} alive, ${result.dead} dead`);
    return result;
  }

  /**
   * Stop proxy testing
   * @returns {Promise<void>}
   */
  async stopTest() {
    await this._send('proxy.test.stop', {});
    logger.info('Proxy test stopped');
    return this;
  }

  /**
   * Get proxy statistics
   * @returns {Promise<object>}
   */
  async stats() {
    const result = await this._send('proxy.stats', {});
    return result;
  }

  /**
   * Get proxy list
   * @returns {Promise<string[]>}
   */
  async list() {
    const result = await this._send('proxy.list', {});
    return result;
  }

  /**
   * Save proxy list to file
   * @param {string} file - Output file path
   * @returns {Promise<void>}
   */
  async save(file) {
    await this._send('proxy.save', { file });
    logger.success(`Proxy list saved: ${file}`);
    return this;
  }

  /**
   * Load OpenVPN configuration
   * @param {string} config - Path to .ovpn file
   * @returns {Promise<void>}
   */
  async ovpn(config) {
    await this._send('proxy.ovpn', { config });
    logger.success(`OpenVPN config loaded: ${config}`);
    return this;
  }

  /**
   * Get/set proxy rotation settings
   * @param {object} opts - Rotation options
   * @returns {Promise<object>}
   */
  async rotation(opts) {
    if (opts) {
      await this._send('proxy.rotation', opts);
      return opts;
    } else {
      return await this._send('proxy.rotation', {});
    }
  }

  /**
   * Get/set proxy configuration
   * @param {object} opts - Config options
   * @returns {Promise<object>}
   */
  async config(opts) {
    if (opts) {
      await this._send('proxy.config', opts);
      return opts;
    } else {
      return await this._send('proxy.config', {});
    }
  }
}

// ── Singleton Instance ──────────────────────────────────────────────────────

const piggy = new Piggy();

// ── Exports ─────────────────────────────────────────────────────────────────

module.exports         = piggy;
module.exports.default = piggy;
module.exports.piggy   = piggy;
module.exports.Piggy   = Piggy;
module.exports.usePiggy = () => piggy;
module.exports.logger  = logger;
