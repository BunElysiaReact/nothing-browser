'use strict';

const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const logger = require('./logger');

/**
 * Resolve the binary path based on mode and platform
 * @param {string} binary - Custom binary path (optional)
 * @param {string} mode - 'headless' or 'headful'
 * @returns {string} Path to binary
 */
function resolveBinary(binary, mode = 'headless') {
  if (binary) {
    logger.debug(`Using custom binary: ${binary}`);
    return path.resolve(binary);
  }

  const platform = process.platform;
  const buildDir = path.join(__dirname, '..', 'build');
  
  let binaryName;
  if (mode === 'headless') {
    binaryName = platform === 'win32' ? 'nothing_headless.exe' : 'nothing_headless';
  } else if (mode === 'headful') {
    binaryName = platform === 'win32' ? 'nothing_headful.exe' : 'nothing_headful';
  } else {
    throw new Error(`Invalid mode: ${mode}. Use 'headless' or 'headful'`);
  }

  const binaryPath = path.join(buildDir, binaryName);
  
  if (!fs.existsSync(binaryPath)) {
    throw new Error(
      `Binary not found at ${binaryPath}. Please build the project first.\n` +
      `Run: cd /workspace && mkdir -p build && cd build && cmake .. && make`
    );
  }

  logger.debug(`Resolved binary: ${binaryPath}`);
  return binaryPath;
}

/**
 * Spawn the Piggy binary
 * @param {string} binPath - Path to binary
 * @param {object} opts - Options
 * @returns {Promise<child_process.ChildProcess>}
 */
async function spawnBinary(binPath, opts = {}) {
  const { args = [], env = {} } = opts;

  logger.info(`Spawning: ${binPath} ${args.join(' ')}`);

  const child = spawn(binPath, args, {
    stdio: ['ignore', 'pipe', 'pipe'],
    env: { ...process.env, ...env },
    detached: false,
  });

  child.stdout.on('data', (data) => {
    const output = data.toString().trim();
    if (output) {
      logger.debug(`[binary] ${output}`);
    }
  });

  child.stderr.on('data', (data) => {
    const output = data.toString().trim();
    if (output) {
      logger.warn(`[binary] ${output}`);
    }
  });

  child.on('error', (err) => {
    logger.error(`Failed to start binary: ${err.message}`);
  });

  child.on('exit', (code) => {
    logger.info(`Binary exited with code ${code}`);
  });

  // Wait for binary to be ready (WebSocket server started)
  await new Promise((resolve) => {
    const checkReady = () => {
      // Binary typically prints "Server started" or similar when ready
      resolve(); // Assume ready after spawn for now
    };
    setTimeout(checkReady, 500);
  });

  return child;
}

/**
 * Detect available binaries
 * @returns {object} Object with available modes
 */
function detectBinary() {
  const buildDir = path.join(__dirname, '..', 'build');
  const platform = process.platform;
  
  const binaries = {
    headless: null,
    headful: null,
  };

  const headlessName = platform === 'win32' ? 'nothing_headless.exe' : 'nothing_headless';
  const headfulName = platform === 'win32' ? 'nothing_headful.exe' : 'nothing_headful';

  const headlessPath = path.join(buildDir, headlessName);
  const headfulPath = path.join(buildDir, headfulName);

  if (fs.existsSync(headlessPath)) {
    binaries.headless = headlessPath;
  }

  if (fs.existsSync(headfulPath)) {
    binaries.headful = headfulPath;
  }

  return binaries;
}

module.exports = {
  resolveBinary,
  spawnBinary,
  detectBinary,
};
