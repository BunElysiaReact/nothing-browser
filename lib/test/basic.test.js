'use strict';

/**
 * Basic Test Script for Piggy Library
 * Tests core functionality: launch, navigation, interactions, data extraction
 */

const piggy = require('../index');
const log = require('../logger');

async function runTests() {
  log.info('Starting Piggy tests...');

  try {
    // Test 1: Launch browser
    log.info('Test 1: Launching headless browser...');
    await piggy.launch({ mode: 'headless' });
    log.success('✓ Browser launched');

    // Test 2: Create a site
    log.info('Test 2: Creating site...');
    const site = await piggy.site('test', { url: 'https://example.com' });
    log.success('✓ Site created');

    // Test 3: Get page title
    log.info('Test 3: Getting page title...');
    const title = await site.pageTitle();
    log.success(`✓ Title: ${title}`);

    // Test 4: Get page URL
    log.info('Test 4: Getting page URL...');
    const url = await site.pageUrl();
    log.success(`✓ URL: ${url}`);

    // Test 5: Check element existence
    log.info('Test 5: Checking element existence...');
    const exists = await site.exists('h1');
    log.success(`✓ H1 exists: ${exists}`);

    // Test 6: Extract text
    log.info('Test 6: Extracting text...');
    const text = await site.text('h1');
    log.success(`✓ H1 text: ${text}`);

    // Test 7: Take screenshot
    log.info('Test 7: Taking screenshot...');
    const screenshotPath = './test_screenshot.png';
    await site.screenshot(screenshotPath);
    log.success(`✓ Screenshot saved: ${screenshotPath}`);

    // Test 8: Navigate to another page
    log.info('Test 8: Navigating to another page...');
    await site.goto('https://httpbin.org/html');
    await site.waitNavigation();
    log.success('✓ Navigation completed');

    // Test 9: Get all links
    log.info('Test 9: Getting all links...');
    const links = await site.links();
    log.success(`✓ Found ${links.length} links`);

    // Test 10: Proxy operations (if available)
    log.info('Test 10: Testing proxy manager...');
    try {
      await piggy.proxy.enable();
      log.success('✓ Proxy enabled');
      
      const currentProxy = await piggy.proxy.current();
      log.info(`Current proxy: ${currentProxy || 'none'}`);
      
      await piggy.proxy.disable();
      log.success('✓ Proxy disabled');
    } catch (err) {
      log.warn(`Proxy test skipped: ${err.message}`);
    }

    // Test 11: Human mode
    log.info('Test 11: Testing human mode...');
    piggy.actHuman(true);
    log.success('✓ Human mode enabled');
    piggy.actHuman(false);
    log.success('✓ Human mode disabled');

    // Cleanup
    log.info('Cleaning up...');
    await piggy.close({ shutdown: true });
    log.success('✓ All tests completed successfully!');

  } catch (error) {
    log.error(`Test failed: ${error.message}`);
    console.error(error.stack);
    
    // Cleanup on error
    try {
      await piggy.close({ force: true });
    } catch (e) {
      // Ignore cleanup errors
    }
    
    process.exit(1);
  }
}

// Run tests
runTests();
