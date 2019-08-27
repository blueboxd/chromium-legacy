// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for managed-footnote. */
cr.define('managed_footnote_test', function() {
  /** @enum {string} */
  const TestNames = {
    Hidden: 'Hidden When isManaged Is False',
    LoadTimeDataBrowser: 'Reads Attributes From loadTimeData browser message',
    Events: 'Responds to is-managed-changed events',
    LoadTimeDataDevice: 'Reads Attributes From loadTimeData device message',
  };

  const suiteName = 'ManagedFootnoteTest';

  suite(suiteName, function() {
    setup(function() {
      PolymerTest.clearBody();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    /**
     * Resets loadTimeData to the parameters, inserts a <managed-footnote>
     * element in the DOM, and returns it.
     * @param {boolean} isManaged Whether the footnote should be visible.
     * @param {string} browserMessage String to display inside the element,
     *     before href substitution.
     * @param {string} deviceMessage String to display inside the element,
     *     before href substitution.
     * @return {HTMLElement}
     */
    function setupTestElement(
        isManaged, browserMessage, deviceMessage, managementPageUrl) {
      loadTimeData.overrideValues({
        chromeManagementUrl: managementPageUrl,
        isManaged: isManaged,
        browserManagedByOrg: browserMessage,
        deviceManagedByOrg: deviceMessage,
      });
      const footnote = document.createElement('managed-footnote');
      document.body.appendChild(footnote);
      Polymer.dom.flush();
      return footnote;
    }

    test('Hidden When isManaged Is False', function() {
      const footnote = setupTestElement(false, '', '', '');
      assertEquals('none', getComputedStyle(footnote).display);
    });

    test('Reads Attributes From loadTimeData browser message', function() {
      const browserMessage = 'the quick brown fox jumps over the lazy dog';
      const deviceMessage = 'the lazy dog jumps over the quick brown fox';
      const footnote = setupTestElement(true, browserMessage, '', '');

      assertNotEquals('none', getComputedStyle(footnote).display);
      assertTrue(footnote.shadowRoot.textContent.includes(browserMessage));
    });

    test('Responds to is-managed-changed events', function() {
      const footnote = setupTestElement(false, '', '', '');
      assertEquals('none', getComputedStyle(footnote).display);

      cr.webUIListenerCallback('is-managed-changed', [true]);
      assertNotEquals('none', getComputedStyle(footnote).display);
    });

    test('Reads Attributes From loadTimeData device message', function() {
      const browserMessage = 'the quick brown fox jumps over the lazy dog';
      const deviceMessage = 'the lazy dog jumps over the quick brown fox';
      const footnote =
          setupTestElement(true, browserMessage, deviceMessage, '');

      assertNotEquals('none', getComputedStyle(footnote).display);
      assertTrue(footnote.shadowRoot.textContent.includes(browserMessage));

      footnote.showDeviceInfo = true;
      assertTrue(footnote.shadowRoot.textContent.includes(deviceMessage));
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
