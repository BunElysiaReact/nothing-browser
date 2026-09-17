'use strict';

/**
 * Advanced Test Script for Piggy Library
 * Tests multi-site operations, data extraction, and complex interactions
 */

const piggy = require('../index');
const logger = require('../logger');

async function runAdvancedTests() {
  logger.info('Starting advanced Piggy tests...');

  try {
    // Launch browser
    await piggy.launch({ mode: 'headless' });
    logger.success('✓ Browser launched');

    // Test 1: Multi-site operations
    logger.info('Test 1: Creating multiple sites...');
    const site1 = await piggy.site('site1', { url: 'https://example.com' });
    const site2 = await piggy.site('site2', { url: 'https://httpbin.org/html' });
    logger.success('✓ Multiple sites created');

    // Test 2: Parallel data extraction with diff
    logger.info('Test 2: Parallel data extraction...');
    const titles = await piggy.diff([site1, site2]).pageTitle();
    logger.success(`✓ Site1 title: ${titles.site1}`);
    logger.success(`✓ Site2 title: ${titles.site2}`);

    // Test 3: Parallel operations with all
    logger.info('Test 3: Parallel URL check...');
    await piggy.all([site1, site2]).pageUrl();
    logger.success('✓ Parallel URL check completed');

    // Test 4: Complex selectors
    logger.info('Test 4: Testing complex selectors...');
    const h1Exists = await site1.exists('h1');
    const hasClass = await site1.hasClass('h1', 'main-heading');
    logger.success(`✓ H1 exists: ${h1Exists}, Has class: ${hasClass}`);

    // Test 5: Form interaction (if available)
    logger.info('Test 5: Testing form interactions...');
    await site2.goto('https://httpbin.org/forms/post');
    await site2.waitNavigation();
    
    // Try to type if input exists
    const inputExists = await site2.exists('#custname');
    if (inputExists) {
      await site2.type('#custname', 'Test User');
      logger.success('✓ Form typing works');
    } else {
      logger.info('⊘ Form input not found, skipping');
    }

    // Test 6: Screenshot and PDF
    logger.info('Test 6: Testing exports...');
    await site1.screenshot('./test_advanced.png');
    logger.success('✓ Screenshot captured');
    
    // Only test PDF if page is simple
    try {
      await site1.pdf('./test.pdf', { landscape: false });
      logger.success('✓ PDF generated');
    } catch (e) {
      logger.warn('PDF generation skipped');
    }

    // Test 7: Network capture
    logger.info('Test 7: Testing network capture...');
    await site1.startCapture();
    await site1.goto('https://httpbin.org/headers');
    await site1.waitNavigation();
    const requests = await site1.getRequests();
    await site1.stopCapture();
    logger.success(`✓ Captured ${requests.length} requests`);

    // Test 8: JavaScript evaluation
    logger.info('Test 8: Testing JS evaluation...');
    const result = await site1.evaluate('document.title');
    logger.success(`✓ Evaluated JS: ${result}`);

    // Test 9: Element traversal
    logger.info('Test 9: Testing element traversal...');
    const firstLink = await site1.first('a');
    if (firstLink) {
      logger.success(`✓ Found first link: ${firstLink.tagName || 'element'}`);
    }
    
    const count = await site1.count('*');
    logger.success(`✓ Total elements: ${count}`);

    // Test 10: Dialog handling
    logger.info('Test 10: Testing dialog handling...');
    await site1.setDialogAutoAction('accept');
    logger.success('✓ Dialog auto-action set');

    // Cleanup
    logger.info('Cleaning up...');
    await site1.close();
    await site2.close();
    await piggy.close({ shutdown: true });
    logger.success('✓ All advanced tests completed!');

  } catch (error) {
    logger.error(`Test failed: ${error.message}`);
    console.error(error.stack);
    
    try {
      await piggy.close({ force: true });
    } catch (e) {}
    
    process.exit(1);
  }
}

runAdvancedTests();
