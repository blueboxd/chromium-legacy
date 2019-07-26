// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  // screenshots in content shell are flaky and NO_NAVSTART occurs on bots way more frequently than local
  // ignore the results of the trace-dependent audits, just make sure they ran
  var FLAKY_AUDITS = [
    // metrics
    'first-contentful-paint',
    'first-meaningful-paint',
    'first-cpu-idle',
    'interactive',
    'estimated-input-latency',
    'speed-index',
    'metrics',
    'screenshot-thumbnails',
    // misc trace-based audits
    'load-fast-enough-for-pwa',
    'user-timings',
    'bootup-time',
    // opportunities
    'efficient-animated-content',
    'offscreen-images',
    'redirects',
    'render-blocking-resources',
    'unminified-css',
    'unminified-javascript',
    'unused-css-rules',
    'uses-optimized-images',
    'uses-rel-preload',
    'uses-responsive-images',
    'uses-text-compression',
    'uses-webp-images',
    // content shell issues
    'content-width' // crbug.com/987722
  ];

  TestRunner.addResult('Tests that audits panel works.\n');

  await TestRunner.loadModule('audits_test_runner');
  await TestRunner.showPanel('audits');

  AuditsTestRunner.dumpStartAuditState();

  TestRunner.addResult(`\n=============== Lighthouse Status Updates ===============`);
  AuditsTestRunner.addStatusListener(msg => TestRunner.addResult(msg));
  AuditsTestRunner.getRunButton().click();

  var results = await AuditsTestRunner.waitForResults();
  TestRunner.addResult(`\n=============== Lighthouse Results ===============`);
  TestRunner.addResult(`URL: ${results.finalUrl}`);
  TestRunner.addResult(`Version: ${results.lighthouseVersion}`);
  TestRunner.addResult('\n');

  Object.keys(results.audits).sort().forEach(auditName => {
    var audit = results.audits[auditName];

    if (FLAKY_AUDITS.includes(auditName)) {
      TestRunner.addResult(`${auditName}: flaky`);
    } else if (audit.scoreDisplayMode === 'error') {
      TestRunner.addResult(`${auditName}: ERROR ${audit.errorMessage}`);
    } else if (audit.scoreDisplayMode === 'binary') {
      TestRunner.addResult(`${auditName}: ${audit.score ? 'pass' : 'fail'}`);
    } else {
      TestRunner.addResult(`${auditName}: ${audit.scoreDisplayMode}`);
    }
  });

  const resultsElement = AuditsTestRunner.getResultsElement();
  const auditElements = resultsElement.querySelectorAll('.lh-audit');
  TestRunner.addResult(`\n# of .lh-audit divs: ${auditElements.length}`);

  TestRunner.completeTest();
})();
