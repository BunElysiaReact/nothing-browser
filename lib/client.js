'use strict';

const WebSocket = require('ws');
const { EventEmitter } = require('events');
const logger = require('./logger');

/**
 * PiggyClient - WebSocket client for communicating with Piggy binary
 * Connects to ws://host:2005 and sends/receives JSON-RPC-like commands
 */
class PiggyClient extends EventEmitter {
  constructor(opts = {}) {
    super();
    this.host = opts.host || '127.0.0.1';
    this.port = 2005;
    this.key = opts.key || null;
    this.ws = null;
    this.pending = new Map(); // id -> { resolve, reject, timeout }
    this.connected = false;
    this.reconnectAttempts = 0;
    this.maxReconnectAttempts = opts.maxReconnectAttempts || 5;
    this.reconnectDelay = opts.reconnectDelay || 1000;
  }

  /**
   * Probe if a Piggy instance is already running on port 2005
   * @returns {Promise<boolean>} true if instance exists and accepts connection
   */
  async probe() {
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(`ws://${this.host}:${this.port}`, {
        headers: this.key ? { 'X-Piggy-Key': this.key } : {},
        handshakeTimeout: 2000,
      });

      ws.on('open', () => {
        ws.close();
        resolve(true);
      });

      ws.on('error', (err) => {
        if (err.message && err.message.includes('403')) {
          const authError = new Error('Authentication failed - wrong key');
          authError.authFailure = true;
          ws.close();
          reject(authError);
        } else {
          resolve(false);
        }
      });

      ws.on('close', () => {
        if (!this.connected) resolve(false);
      });
    });
  }

  /**
   * Connect to Piggy binary
   * @returns {Promise<PiggyClient>}
   */
  async connect() {
    return new Promise((resolve, reject) => {
      const url = `ws://${this.host}:${this.port}`;
      logger.debug(`Connecting to ${url}`);

      const options = {
        headers: this.key ? { 'X-Piggy-Key': this.key } : {},
      };

      // Clean up old socket if exists
      if (this.ws) {
        this.ws.removeAllListeners();
        if (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING) {
          this.ws.close();
        }
      }

      this.ws = new WebSocket(url, options);

      // Connection timeout
      const connectTimeout = setTimeout(() => {
        if (!this.connected) {
          logger.error('Connection timed out');
          reject(new Error('Connection timed out after 10s'));
        }
      }, 10000);

      this.ws.on('open', () => {
        clearTimeout(connectTimeout);
        this.connected = true;
        this.reconnectAttempts = 0;
        logger.success(`Connected to Piggy at ${this.host}:${this.port}`);
        resolve(this);
      });

      this.ws.on('message', (data) => {
        try {
          const msg = JSON.parse(data.toString());
          this._handleMessage(msg);
        } catch (err) {
          logger.error(`Failed to parse message: ${err.message}`);
        }
      });

      this.ws.on('error', (err) => {
        logger.error(`WebSocket error: ${err.message}`);
        this.connected = false;
        if (this.pending.size > 0) {
          for (const [id, pending] of this.pending.entries()) {
            pending.reject(err);
          }
          this.pending.clear();
        }
        // Only reject during initial connection, not on runtime errors
        if (!this.connected && this.ws.readyState !== WebSocket.OPEN) {
          reject(err);
        }
      });

      this.ws.on('close', () => {
        logger.warn('WebSocket closed');
        this.connected = false;
        this.emit('disconnected');
        
        // Don't reconnect if this was an intentional disconnect
        if (this._intentionalClose) {
          this._intentionalClose = false;
          return;
        }
        
        // Auto-reconnect logic with exponential backoff
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
          this.reconnectAttempts++;
          const delay = this.reconnectDelay * this.reconnectAttempts;
          logger.info(`Reconnecting (${this.reconnectAttempts}/${this.maxReconnectAttempts}) in ${delay}ms...`);
          setTimeout(() => this.connect().catch(() => {}), delay);
        }
      });
    });
  }

  /**
   * Handle incoming messages
   * @private
   */
  _handleMessage(msg) {
    if (msg.type === 'event') {
      // Broadcast event to listeners
      this.emit(msg.event, msg.data);
    } else if (msg.type === 'response') {
      // Resolve pending command
      const pending = this.pending.get(msg.id);
      if (pending) {
        clearTimeout(pending.timeout);
        this.pending.delete(msg.id);
        if (msg.success) {
          pending.resolve(msg.data);
        } else {
          pending.reject(new Error(msg.error || 'Command failed'));
        }
      }
    }
  }

  /**
   * Send a command and wait for response
   * @param {string} cmd - Command name (e.g., "tab.new", "navigate")
   * @param {object} payload - Command payload
   * @param {number} timeout - Timeout in ms (default: 30000)
   * @returns {Promise<any>}
   */
  async send(cmd, payload = {}, timeout = 30000) {
    return new Promise((resolve, reject) => {
      const id = `${cmd}-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
      
      const message = {
        id,
        cmd,
        payload,
      };

      const timeoutId = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`Command '${cmd}' timed out after ${timeout}ms`));
      }, timeout);

      this.pending.set(id, {
        resolve,
        reject,
        timeout: timeoutId,
      });

      if (this.ws && this.ws.readyState === WebSocket.OPEN) {
        this.ws.send(JSON.stringify(message));
      } else {
        clearTimeout(timeoutId);
        this.pending.delete(id);
        reject(new Error('WebSocket not connected'));
      }
    });
  }

  /**
   * Disconnect from Piggy binary
   */
  disconnect() {
    this._intentionalClose = true;
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
    this.connected = false;
    
    // Reject all pending commands
    for (const [id, pending] of this.pending.entries()) {
      clearTimeout(pending.timeout);
      pending.reject(new Error('Disconnected'));
    }
    this.pending.clear();
    
    logger.info('Disconnected from Piggy');
  }
}

module.exports = { PiggyClient };
