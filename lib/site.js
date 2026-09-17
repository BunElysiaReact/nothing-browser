'use strict';

const { PiggyClient } = require('./client');
const logger = require('./logger');

/**
 * Site - High-level abstraction for a single website/tab
 * Provides chainable API for browser automation
 */
class Site {
  constructor(client, name, tabId) {
    this._client = client;
    this._name = name;
    this._tabId = tabId || 'default';
    this._timeout = 30000;
  }

  /**
   * Set timeout for operations
   * @param {number} ms - Timeout in milliseconds
   * @returns {Site}
   */
  timeout(ms) {
    this._timeout = ms;
    return this;
  }

  // ─── Navigation ─────────────────────────────────────────────────────────────

  /**
   * Navigate to a URL
   * @param {string} url - URL to navigate to
   * @returns {Promise<Site>}
   */
  async goto(url) {
    await this._client.send('navigate', { url: url, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Navigated to ${url}`);
    return this;
  }

  /**
   * Refresh current page
   * @returns {Promise<Site>}
   */
  async refresh() {
    await this._client.send('refresh', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Refreshed page`);
    return this;
  }

  /**
   * Reload page (alias for refresh)
   * @returns {Promise<Site>}
   */
  async reload() {
    return this.refresh();
  }

  /**
   * Go back in history
   * @returns {Promise<Site>}
   */
  async back() {
    await this._client.send('go.back', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Went back`);
    return this;
  }

  /**
   * Go forward in history
   * @returns {Promise<Site>}
   */
  async forward() {
    await this._client.send('go.forward', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Went forward`);
    return this;
  }

  /**
   * Wait for navigation to complete
   * @returns {Promise<Site>}
   */
  async waitNavigation() {
    await this._client.send('wait.navigation', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Waited for navigation`);
    return this;
  }

  /**
   * Wait for a selector to appear
   * @param {string} selector - CSS selector
   * @returns {Promise<Site>}
   */
  async waitSelector(selector) {
    await this._client.send('wait.selector', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Waited for selector: ${selector}`);
    return this;
  }

  /**
   * Wait for a response matching a pattern
   * @param {string} pattern - URL pattern to match
   * @returns {Promise<Site>}
   */
  async waitResponse(pattern) {
    await this._client.send('wait.response', { pattern, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Waited for response: ${pattern}`);
    return this;
  }

  // ─── Interactions ───────────────────────────────────────────────────────────

  /**
   * Click on an element
   * @param {string} selector - CSS selector
   * @returns {Promise<Site>}
   */
  async click(selector) {
    await this._client.send('click', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Clicked: ${selector}`);
    return this;
  }

  /**
   * Double click on an element
   * @param {string} selector - CSS selector
   * @returns {Promise<Site>}
   */
  async dblclick(selector) {
    await this._client.send('dblclick', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Double clicked: ${selector}`);
    return this;
  }

  /**
   * Hover over an element
   * @param {string} selector - CSS selector
   * @returns {Promise<Site>}
   */
  async hover(selector) {
    await this._client.send('hover', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Hovered: ${selector}`);
    return this;
  }

  /**
   * Type text into an input
   * @param {string} selector - CSS selector
   * @param {string} text - Text to type
   * @returns {Promise<Site>}
   */
  async type(selector, text) {
    await this._client.send('type', { selector, text, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Typed into ${selector}: ${text.substring(0, 20)}...`);
    return this;
  }

  /**
   * Select an option from a dropdown
   * @param {string} selector - CSS selector
   * @param {string|array} value - Option value(s)
   * @returns {Promise<Site>}
   */
  async select(selector, value) {
    const values = Array.isArray(value) ? value : [value];
    await this._client.send('select', { selector, values, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Selected in ${selector}: ${values.join(', ')}`);
    return this;
  }

  /**
   * Upload a file
   * @param {string} selector - CSS selector for file input
   * @param {string|array} filePath - File path(s)
   * @returns {Promise<Site>}
   */
  async upload(selector, filePath) {
    const paths = Array.isArray(filePath) ? filePath : [filePath];
    await this._client.send('upload', { selector, paths, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Uploaded to ${selector}: ${paths.join(', ')}`);
    return this;
  }

  /**
   * Scroll to an element
   * @param {string} selector - CSS selector
   * @returns {Promise<Site>}
   */
  async scrollTo(selector) {
    await this._client.send('scroll.to', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Scrolled to: ${selector}`);
    return this;
  }

  /**
   * Scroll by x, y pixels
   * @param {number} x - Horizontal pixels
   * @param {number} y - Vertical pixels
   * @returns {Promise<Site>}
   */
  async scrollBy(x, y) {
    await this._client.send('scroll.by', { x, y, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Scrolled by: ${x}, ${y}`);
    return this;
  }

  /**
   * Perform a mouse drag operation
   * @param {string} from - Source selector
   * @param {string} to - Destination selector
   * @returns {Promise<Site>}
   */
  async drag(from, to) {
    await this._client.send('mouse.drag', { from, to, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Dragged from ${from} to ${to}`);
    return this;
  }

  /**
   * Move mouse to coordinates
   * @param {number} x - X coordinate
   * @param {number} y - Y coordinate
   * @returns {Promise<Site>}
   */
  async mouseMove(x, y) {
    await this._client.send('mouse.move', { x, y, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Mouse moved to: ${x}, ${y}`);
    return this;
  }

  /**
   * Press a keyboard key
   * @param {string} key - Key name (e.g., "Enter", "Escape")
   * @returns {Promise<Site>}
   */
  async press(key) {
    await this._client.send('keyboard.press', { key, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Pressed key: ${key}`);
    return this;
  }

  /**
   * Press a keyboard combination
   * @param {array} keys - Array of keys (e.g., ["Control", "C"])
   * @returns {Promise<Site>}
   */
  async combo(keys) {
    await this._client.send('keyboard.combo', { keys, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Pressed combo: ${keys.join('+')}`);
    return this;
  }

  // ─── Dialogs ────────────────────────────────────────────────────────────────

  /**
   * Accept a dialog (alert/confirm/prompt)
   * @param {string} promptText - Optional text for prompts
   * @returns {Promise<Site>}
   */
  async acceptDialog(promptText) {
    await this._client.send('dialog.accept', { promptText, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Accepted dialog`);
    return this;
  }

  /**
   * Dismiss a dialog
   * @returns {Promise<Site>}
   */
  async dismissDialog() {
    await this._client.send('dialog.dismiss', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Dismissed dialog`);
    return this;
  }

  /**
   * Set auto-action for dialogs
   * @param {string} action - "accept" or "dismiss"
   * @returns {Promise<Site>}
   */
  async setDialogAutoAction(action) {
    await this._client.send('dialog.setAutoAction', { action, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Set dialog auto-action: ${action}`);
    return this;
  }

  /**
   * Wait for and accept a dialog
   * @returns {Promise<Site>}
   */
  async waitAndAcceptDialog() {
    await this._client.send('dialog.waitAndAccept', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Waited and accepted dialog`);
    return this;
  }

  /**
   * Wait for and dismiss a dialog
   * @returns {Promise<Site>}
   */
  async waitAndDismissDialog() {
    await this._client.send('dialog.waitAndDismiss', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Waited and dismissed dialog`);
    return this;
  }

  /**
   * Get dialog status
   * @returns {Promise<object>}
   */
  async getDialogStatus() {
    const result = await this._client.send('dialog.status', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Got dialog status`);
    return result;
  }

  // ─── Find/Query Elements ────────────────────────────────────────────────────

  /**
   * Check if an element exists
   * @param {string} selector - CSS selector
   * @returns {Promise<boolean>}
   */
  async exists(selector) {
    const result = await this._client.send('find.exists', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Checked existence: ${selector} = ${result}`);
    return result;
  }

  /**
   * Check if an element is visible
   * @param {string} selector - CSS selector
   * @returns {Promise<boolean>}
   */
  async visible(selector) {
    const result = await this._client.send('find.visible', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Checked visibility: ${selector} = ${result}`);
    return result;
  }

  /**
   * Check if an element has a class
   * @param {string} selector - CSS selector
   * @param {string} className - Class name
   * @returns {Promise<boolean>}
   */
  async hasClass(selector, className) {
    const result = await this._client.send('find.hasClass', { selector, className, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Checked class: ${selector}.${className} = ${result}`);
    return result;
  }

  /**
   * Check if an element has an attribute
   * @param {string} selector - CSS selector
   * @param {string} attr - Attribute name
   * @returns {Promise<boolean>}
   */
  async hasAttr(selector, attr) {
    const result = await this._client.send('find.hasAttr', { selector, attr, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Checked attribute: ${selector}[${attr}] = ${result}`);
    return result;
  }

  /**
   * Check if an element has text content
   * @param {string} selector - CSS selector
   * @param {string} text - Text to search for
   * @returns {Promise<boolean>}
   */
  async hasText(selector, text) {
    const result = await this._client.send('find.hasText', { selector, text, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Checked text: ${selector} contains "${text}" = ${result}`);
    return result;
  }

  /**
   * Check if an element matches a selector
   * @param {string} selector - CSS selector
   * @returns {Promise<boolean>}
   */
  async matches(selector) {
    const result = await this._client.send('find.matches', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Checked matches: ${selector} = ${result}`);
    return result;
  }

  /**
   * Check if an element is checked (for checkboxes/radios)
   * @param {string} selector - CSS selector
   * @returns {Promise<boolean>}
   */
  async checked(selector) {
    const result = await this._client.send('find.checked', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Checked state: ${selector} = ${result}`);
    return result;
  }

  /**
   * Check if an element is enabled
   * @param {string} selector - CSS selector
   * @returns {Promise<boolean>}
   */
  async enabled(selector) {
    const result = await this._client.send('find.enabled', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Checked enabled: ${selector} = ${result}`);
    return result;
  }

  // ─── Provide/Extract Data ───────────────────────────────────────────────────

  /**
   * Get text content of an element
   * @param {string} selector - CSS selector
   * @returns {Promise<string>}
   */
  async text(selector) {
    const result = await this._client.send('provide.text', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted text from: ${selector}`);
    return result;
  }

  /**
   * Get text content of all matching elements
   * @param {string} selector - CSS selector
   * @returns {Promise<string[]>}
   */
  async textAll(selector) {
    const result = await this._client.send('provide.textAll', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted text from all: ${selector}`);
    return result;
  }

  /**
   * Get HTML content of an element
   * @param {string} selector - CSS selector
   * @returns {Promise<string>}
   */
  async html(selector) {
    const result = await this._client.send('provide.html', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted HTML from: ${selector}`);
    return result;
  }

  /**
   * Get attribute value of an element
   * @param {string} selector - CSS selector
   * @param {string} attr - Attribute name
   * @returns {Promise<string>}
   */
  async attr(selector, attr) {
    const result = await this._client.send('provide.attr', { selector, attr, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted attribute ${attr} from: ${selector}`);
    return result;
  }

  /**
   * Get attribute values of all matching elements
   * @param {string} selector - CSS selector
   * @param {string} attr - Attribute name
   * @returns {Promise<string[]>}
   */
  async attrAll(selector, attr) {
    const result = await this._client.send('provide.attrAll', { selector, attr, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted attribute ${attr} from all: ${selector}`);
    return result;
  }

  /**
   * Get all links on the page
   * @returns {Promise<object[]>}
   */
  async links() {
    const result = await this._client.send('provide.links', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted links`);
    return result;
  }

  /**
   * Get all images on the page
   * @returns {Promise<object[]>}
   */
  async images() {
    const result = await this._client.send('provide.images', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted images`);
    return result;
  }

  /**
   * Get form data
   * @param {string} selector - Form selector
   * @returns {Promise<object>}
   */
  async form(selector) {
    const result = await this._client.send('provide.form', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted form: ${selector}`);
    return result;
  }

  /**
   * Get table data
   * @param {string} selector - Table selector
   * @returns {Promise<object[]>}
   */
  async table(selector) {
    const result = await this._client.send('provide.table', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted table: ${selector}`);
    return result;
  }

  /**
   * Get list items
   * @param {string} selector - List selector
   * @returns {Promise<string[]>}
   */
  async list(selector) {
    const result = await this._client.send('provide.list', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted list: ${selector}`);
    return result;
  }

  /**
   * Get meta tags
   * @returns {Promise<object>}
   */
  async meta() {
    const result = await this._client.send('provide.meta', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted meta tags`);
    return result;
  }

  /**
   * Get JSON-LD or structured data
   * @returns {Promise<object>}
   */
  async json() {
    const result = await this._client.send('provide.json', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted JSON data`);
    return result;
  }

  /**
   * Get page content
   * @returns {Promise<string>}
   */
  async pageContent() {
    const result = await this._client.send('page.content', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted page content`);
    return result;
  }

  /**
   * Get page title
   * @returns {Promise<string>}
   */
  async pageTitle() {
    const result = await this._client.send('page.title', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted page title`);
    return result;
  }

  /**
   * Get current URL
   * @returns {Promise<string>}
   */
  async pageUrl() {
    const result = await this._client.send('page.url', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted page URL`);
    return result;
  }

  /**
   * Find element by attribute
   * @param {string} attr - Attribute name
   * @param {string} value - Attribute value
   * @returns {Promise<object>}
   */
  async byAttr(attr, value) {
    const result = await this._client.send('provide.byAttr', { attr, value, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Found by attribute: [${attr}="${value}"]`);
    return result;
  }

  /**
   * Find element by placeholder text
   * @param {string} placeholder - Placeholder text
   * @returns {Promise<object>}
   */
  async byPlaceholder(placeholder) {
    const result = await this._client.send('provide.byPlaceholder', { placeholder, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Found by placeholder: "${placeholder}"`);
    return result;
  }

  /**
   * Find element by ARIA role
   * @param {string} role - ARIA role
   * @returns {Promise<object>}
   */
  async byRole(role) {
    const result = await this._client.send('provide.byRole', { role, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Found by role: ${role}`);
    return result;
  }

  /**
   * Find element by tag name
   * @param {string} tag - Tag name
   * @returns {Promise<object>}
   */
  async byTag(tag) {
    const result = await this._client.send('provide.byTag', { tag, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Found by tag: ${tag}`);
    return result;
  }

  /**
   * Get children of an element
   * @param {string} selector - Parent selector
   * @returns {Promise<object[]>}
   */
  async children(selector) {
    const result = await this._client.send('provide.children', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted children of: ${selector}`);
    return result;
  }

  /**
   * Get closest ancestor matching selector
   * @param {string} selector - Child selector
   * @param {string} ancestor - Ancestor selector
   * @returns {Promise<object>}
   */
  async closest(selector, ancestor) {
    const result = await this._client.send('provide.closest', { selector, ancestor, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Found closest ancestor`);
    return result;
  }

  /**
   * Get parent of an element
   * @param {string} selector - Element selector
   * @returns {Promise<object>}
   */
  async parent(selector) {
    const result = await this._client.send('provide.parent', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted parent of: ${selector}`);
    return result;
  }

  /**
   * Count elements matching selector
   * @param {string} selector - CSS selector
   * @returns {Promise<number>}
   */
  async count(selector) {
    const result = await this._client.send('provide.count', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Counted elements: ${selector} = ${result}`);
    return result;
  }

  /**
   * Get first element matching selector
   * @param {string} selector - CSS selector
   * @returns {Promise<object>}
   */
  async first(selector) {
    const result = await this._client.send('provide.first', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Got first element: ${selector}`);
    return result;
  }

  /**
   * Filter elements by condition
   * @param {string} selector - CSS selector
   * @param {object} options - Filter options
   * @returns {Promise<object[]>}
   */
  async filter(selector, options) {
    const result = await this._client.send('provide.filter', { selector, options, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Filtered elements: ${selector}`);
    return result;
  }

  /**
   * Select elements (alias for provide.select)
   * @param {string} selector - CSS selector
   * @returns {Promise<object[]>}
   */
  async selectElements(selector) {
    const result = await this._client.send('provide.select', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Selected elements: ${selector}`);
    return result;
  }

  /**
   * Get div content
   * @param {string} selector - Div selector
   * @returns {Promise<string>}
   */
  async div(selector) {
    const result = await this._client.send('provide.div', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Extracted div: ${selector}`);
    return result;
  }

  /**
   * Evaluate JavaScript on the page
   * @param {string} script - JavaScript code
   * @returns {Promise<any>}
   */
  async evaluate(script) {
    const result = await this._client.send('evaluate', { script, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Evaluated script`);
    return result;
  }

  // ─── Screenshots & PDF ──────────────────────────────────────────────────────

  /**
   * Take a screenshot
   * @param {string} path - File path to save screenshot
   * @param {object} options - Screenshot options (fullPage, clip, etc.)
   * @returns {Promise<string>} Path to saved file
   */
  async screenshot(path, options = {}) {
    const result = await this._client.send('screenshot', { path, ...options, tabId: this._tabId }, this._timeout);
    logger.success(`[${this._name}] Screenshot saved: ${path}`);
    return result;
  }

  /**
   * Save page as PDF
   * @param {string} path - File path to save PDF
   * @param {object} options - PDF options (landscape, printBackground, etc.)
   * @returns {Promise<string>} Path to saved file
   */
  async pdf(path, options = {}) {
    const result = await this._client.send('pdf', { path, ...options, tabId: this._tabId }, this._timeout);
    logger.success(`[${this._name}] PDF saved: ${path}`);
    return result;
  }

  /**
   * Export page data as JSON
   * @param {string} path - File path
   * @param {object} data - Data to export
   * @returns {Promise<string>}
   */
  async exportJson(path, data) {
    const result = await this._client.send('export.json', { path, data, tabId: this._tabId }, this._timeout);
    logger.success(`[${this._name}] JSON exported: ${path}`);
    return result;
  }

  // ─── Capture (Network/WebSocket/Cookies) ───────────────────────────────────

  /**
   * Start capturing network requests
   * @returns {Promise<Site>}
   */
  async startCapture() {
    await this._client.send('capture.start', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Started capture`);
    return this;
  }

  /**
   * Stop capturing
   * @returns {Promise<Site>}
   */
  async stopCapture() {
    await this._client.send('capture.stop', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Stopped capture`);
    return this;
  }

  /**
   * Clear captured data
   * @returns {Promise<Site>}
   */
  async clearCapture() {
    await this._client.send('capture.clear', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Cleared capture`);
    return this;
  }

  /**
   * Get captured requests
   * @returns {Promise<object[]>}
   */
  async getRequests() {
    const result = await this._client.send('capture.requests', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Got captured requests`);
    return result;
  }

  /**
   * Get captured WebSocket frames
   * @returns {Promise<object[]>}
   */
  async getWsFrames() {
    const result = await this._client.send('capture.ws', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Got captured WebSocket frames`);
    return result;
  }

  // ─── Intercept (Block/Modify Requests) ─────────────────────────────────────

  /**
   * Block image loading
   * @returns {Promise<Site>}
   */
  async blockImages() {
    await this._client.send('intercept.block.images', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Blocked images`);
    return this;
  }

  /**
   * Unblock image loading
   * @returns {Promise<Site>}
   */
  async unblockImages() {
    await this._client.send('intercept.unblock.images', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Unblocked images`);
    return this;
  }

  // ─── Media (Audio/Video) ────────────────────────────────────────────────────

  /**
   * Play media (audio/video)
   * @param {string} selector - Media element selector
   * @returns {Promise<Site>}
   */
  async playMedia(selector) {
    await this._client.send('media.play', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Played media: ${selector}`);
    return this;
  }

  /**
   * Pause media
   * @param {string} selector - Media element selector
   * @returns {Promise<Site>}
   */
  async pauseMedia(selector) {
    await this._client.send('media.pause', { selector, tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Paused media: ${selector}`);
    return this;
  }

  /**
   * Mute media
   * @returns {Promise<Site>}
   */
  async muteMedia() {
    await this._client.send('media.mute', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Muted media`);
    return this;
  }

  /**
   * Unmute media
   * @returns {Promise<Site>}
   */
  async unmuteMedia() {
    await this._client.send('media.unmute', { tabId: this._tabId }, this._timeout);
    logger.debug(`[${this._name}] Unmuted media`);
    return this;
  }

  // ─── Close ──────────────────────────────────────────────────────────────────

  /**
   * Close the site/tab
   * @returns {Promise<void>}
   */
  async close() {
    if (this._tabId && this._tabId !== 'default') {
      await this._client.send('tab.close', { tabId: this._tabId }, this._timeout);
      logger.info(`[${this._name}] Closed tab: ${this._tabId}`);
    }
  }
}

module.exports = { createSite };

/**
 * Create a new Site instance
 * @param {PiggyClient} client - PiggyClient instance
 * @param {string} name - Site name for logging
 * @param {string} tabId - Tab ID (optional, defaults to 'default')
 * @returns {Site}
 */
function createSite(client, name, tabId) {
  return new Site(client, name, tabId);
}
