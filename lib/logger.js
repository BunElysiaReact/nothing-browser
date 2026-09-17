'use strict';

const ernestLogger = require('ernest-logger');

// Create logger instance
const rawLogger = ernestLogger({
  time:   true,
  file:   false, // Disable file logging for now
  prefix: '[PIGGY]',
  emoji:  true,
});

// Wrap ernest-logger to provide familiar API
const logger = {
  info(msg) {
    rawLogger.log.info.blue(`${msg}`);
  },
  
  success(msg) {
    rawLogger.log.success.green(`${msg}`);
  },
  
  error(msg) {
    rawLogger.log.error.red(`${msg}`);
  },
  
  warn(msg) {
    rawLogger.log.warn.yellow(`${msg}`);
  },
  
  debug(msg) {
    rawLogger.log.debug.cyan(`${msg}`);
  },
  
  network(msg) {
    rawLogger.log.info.brightCyan(`${msg}`);
  },
  
  db(msg) {
    rawLogger.log.info.brightMagenta(`${msg}`);
  },
  
  security(msg) {
    rawLogger.log.error.brightRed(`${msg}`);
  },
};

module.exports = logger;
