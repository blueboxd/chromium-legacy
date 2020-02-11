// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings basic page. */

// Register mocha tests.
suite('SettingsBasicPage', function() {
  let page = null;

  setup(function() {
    PolymerTest.clearBody();
    page = document.createElement('settings-basic-page');
    document.body.appendChild(page);
  });

  test('load page', function() {
    // This will fail if there are any asserts or errors in the Settings page.
  });

  test('basic pages', function() {
    const sections =
        ['appearance', 'onStartup', 'people', 'search', 'autofill', 'privacy'];
    if (!cr.isChromeOS) {
      sections.push('defaultBrowser');
    }

    Polymer.dom.flush();

    for (const section of sections) {
      const sectionElement = page.$$(`settings-section[section=${section}]`);
      assertTrue(!!sectionElement);
    }
  });
});
