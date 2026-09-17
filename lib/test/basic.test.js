'use strict';

/**
 * Basic Test Script for Piggy Library
 * Tests core functionality: launch, navigation, interactions, data extraction
 */

const piggy = require('../index');
const logger = require('../logger');

async function runTests() {
  logger.info('Starting Piggy tests...');

  try {
    // Test 1: Launch browser
    logger.info('Test 1: Launching headless browser...');
    await piggy.launch({ mode: 'headless' });
    logger.success('✓ Browser launched');

    // Test 2: Create a site
    logger.info('Test 2: Creating site...');
    const site = await piggy.site('test', { url: 'https://example.com' });
    logger.success('✓ Site created');

    // Test 3: Get page title
    logger.info('Test 3: Getting page title...');
    const title = await site.pageTitle();
    logger.success(`✓ Title: ${title}`);

    // Test 4: Get page URL
    logger.info('Test 4: Getting page URL...');
    const url = await site.pageUrl();
    logger.success(`✓ URL: ${url}`);

    // Test 5: Check element existence
    logger.info('Test 5: Checking element existence...');
    const exists = await site.exists('h1');
    logger.success(`✓ H1 exists: ${exists}`);

    // Test 6: Extract text
    logger.info('Test 6: Extracting text...');
    const text = await site.text('h1');
    logger.success(`✓ H1 text: ${text}`);

    // Test 7: Take screenshot
    logger.info('Test 7: Taking screenshot...');
    const screenshotPath = './test_screenshot.png';
    await site.screenshot(screenshotPath);
    logger.success(`✓ Screenshot saved: ${screenshotPath}`);

    // Test 8: Navigate to another page
    logger.info('Test 8: Navigating to another page...');
    await site.goto('https://httpbin.org/html');
    await site.waitNavigation();
    logger.success('✓ Navigation completed');

    // Test 9: Get all links
    logger.info('Test 9: Getting all links...');
    const links = await site.links();
    logger.success(`✓ Found ${links.length} links`);

    // Test 10: Proxy operations (if available)
    logger.info('Test 10: Testing proxy manager...');
    try {
      await piggy.proxy.enable();
      logger.success('✓ Proxy enabled');
      
      const currentProxy = await piggy.proxy.current();
      logger.info(`Current proxy: ${currentProxy || 'none'}`);
      
      await piggy.proxy.disable();
      logger.success('✓ Proxy disabled');
    } catch (err) {
      logger.warn(`Proxy test skipped: ${err.message}`);
    }

    // Test 11: Human mode
    logger.info('Test 11: Testing human mode...');
    piggy.actHuman(true);
    logger.success('✓ Human mode enabled');
    piggy.actHuman(false);
    logger.success('✓ Human mode disabled');

    // Cleanup
    logger.info('Cleaning up...');
    await piggy.close({ shutdown: true });
    logger.success('✓ All tests completed successfully!');

  } catch (error) {
    logger.error(`Test failed: ${error.message}`);
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
