// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, getSelectDropdownBackground} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {getGoogleDriveDestination, selectOption} from 'chrome://test/print_preview/print_preview_test_utils.js';

window.destination_select_test_cros = {};
destination_select_test_cros.suiteName = 'DestinationSelectTestCros';
/** @enum {string} */
destination_select_test_cros.TestNames = {
  UpdateStatus: 'update status',
  ChangeIcon: 'change icon',
  EulaIsDisplayed: 'eula is displayed'
};

suite(destination_select_test_cros.suiteName, function() {
  /** @type {?PrintPreviewDestinationSelectElement} */
  let destinationSelect = null;

  const account = 'foo@chromium.org';

  let recentDestinationList = [];

  /** @override */
  setup(function() {
    PolymerTest.clearBody();

    destinationSelect =
        document.createElement('print-preview-destination-select-cros');
    destinationSelect.activeUser = account;
    destinationSelect.appKioskMode = false;
    destinationSelect.disabled = false;
    destinationSelect.loaded = false;
    destinationSelect.noDestinations = false;
    recentDestinationList = [
      new Destination(
          'ID1', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'One',
          DestinationConnectionStatus.ONLINE),
      new Destination(
          'ID2', DestinationType.CLOUD, DestinationOrigin.COOKIES, 'Two',
          DestinationConnectionStatus.OFFLINE, {account: account}),
      new Destination(
          'ID3', DestinationType.CLOUD, DestinationOrigin.COOKIES, 'Three',
          DestinationConnectionStatus.ONLINE,
          {account: account, isOwned: true}),
    ];
    destinationSelect.recentDestinationList = recentDestinationList;

    document.body.appendChild(destinationSelect);
  });

  function compareIcon(selectEl, expectedIcon) {
    const icon = selectEl.style['background-image'].replace(/ /gi, '');
    const expected = getSelectDropdownBackground(
        destinationSelect.meta_.byKey('print-preview'), expectedIcon,
        destinationSelect);
    assertEquals(expected, icon);
  }

  test(assert(destination_select_test_cros.TestNames.UpdateStatus), function() {
    assertFalse(destinationSelect.$$('.throbber-container').hidden);
    assertTrue(destinationSelect.$$('.md-select').hidden);

    destinationSelect.loaded = true;
    assertTrue(destinationSelect.$$('.throbber-container').hidden);
    assertFalse(destinationSelect.$$('.md-select').hidden);

    destinationSelect.destination = recentDestinationList[0];
    destinationSelect.updateDestination();
    assertTrue(destinationSelect.$$('.destination-additional-info').hidden);

    destinationSelect.destination = recentDestinationList[1];
    destinationSelect.updateDestination();
    assertFalse(destinationSelect.$$('.destination-additional-info').hidden);
  });

  test(assert(destination_select_test_cros.TestNames.ChangeIcon), function() {
    const destination = recentDestinationList[0];
    destinationSelect.destination = destination;
    destinationSelect.updateDestination();
    destinationSelect.loaded = true;
    const selectEl = destinationSelect.$$('.md-select');
    compareIcon(selectEl, 'print');
    const cookieOrigin = DestinationOrigin.COOKIES;
    const driveKey =
        `${Destination.GooglePromotedId.DOCS}/${cookieOrigin}/${account}`;
    destinationSelect.driveDestinationKey = driveKey;

    return selectOption(destinationSelect, driveKey)
        .then(() => {
          // Icon updates early based on the ID.
          compareIcon(selectEl, 'save-to-drive');

          // Update the destination.
          destinationSelect.destination = getGoogleDriveDestination(account);

          // Still Save to Drive icon.
          compareIcon(selectEl, 'save-to-drive');

          // Select a destination with the shared printer icon.
          return selectOption(
              destinationSelect, `ID2/${cookieOrigin}/${account}`);
        })
        .then(() => {
          // Should already be updated.
          compareIcon(selectEl, 'printer-shared');

          // Update destination.
          destinationSelect.destination = recentDestinationList[1];
          compareIcon(selectEl, 'printer-shared');

          // Select a destination with a standard printer icon.
          return selectOption(
              destinationSelect, `ID3/${cookieOrigin}/${account}`);
        })
        .then(() => {
          compareIcon(selectEl, 'print');
        });
  });

  /**
   * Tests that destinations with a EULA will display the EULA URL.
   */
  test(
      assert(destination_select_test_cros.TestNames.EulaIsDisplayed),
      function() {
        destinationSelect.destination = recentDestinationList[0];
        destinationSelect.loaded = true;
        assertTrue(destinationSelect.$.destinationEulaWrapper.hidden);

        destinationSelect.set(
            'destination.eulaUrl', 'chrome://os-credits/eula');
        const eulaWrapper = destinationSelect.$.destinationEulaWrapper;
        assertFalse(destinationSelect.$.destinationEulaWrapper.hidden);
      });
});
