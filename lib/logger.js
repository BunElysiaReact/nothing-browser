'use strict';

const { createLogger } = require('ernest-logger');

const logger = createLogger({
  time:     true,
  file:     true,
  filePath: './piggy.log',
  prefix:   '[PIGGY]',
  emoji:    true,
  level:    'trace',
  customLevels: {
    info:     { color: 'blue',          emoji: 'ℹ️',  priority: 2 },
    success:  { color: 'green',         emoji: '✅',  priority: 2 },
    error:    { color: 'red',           emoji: '❌',  priority: 5 },
    warn:     { color: 'yellow',        emoji: '⚠️',  priority: 4 },
    debug:    { color: 'magenta',       emoji: '🐞',  priority: 1 },
    network:  { color: 'brightCyan',    emoji: '🌐',  priority: 2 },
    db:       { color: 'brightMagenta', emoji: '🗄️',  priority: 2 },
    security: { color: 'brightRed',     emoji: '🔒',  priority: 3 },
  },
});

module.exports = logger;
