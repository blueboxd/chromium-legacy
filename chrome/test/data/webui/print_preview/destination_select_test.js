// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_select_test', function() {
  /** @enum {string} */
  const TestNames = {
    ChangeIcon: 'change icon',
  };

  const suiteName = 'DestinationSelectTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewDestinationSelectElement} */
    let destinationSelect = null;

    const account = 'foo@chromium.org';

    /** @override */
    setup(function() {
      PolymerTest.clearBody();

      destinationSelect =
          document.createElement('print-preview-destination-select');
      destinationSelect.activeUser = account;
      destinationSelect.appKioskMode = false;
      destinationSelect.disabled = false;
      destinationSelect.noDestinations = false;
      destinationSelect.recentDestinationList = [
        // Local printer without stickied icon
        {
          id: 'ID1',
          origin: print_preview.DestinationOrigin.LOCAL,
          account: '',
          capabilities: null,
          displayName: 'One',
          extensionId: '',
          extensionName: ''
        },
        // Shared cloud printer with stickied icon
        {
          id: 'ID2',
          origin: print_preview.DestinationOrigin.COOKIES,
          account: account,
          capabilities: null,
          displayName: 'Two',
          extensionId: '',
          extensionName: '',
          icon: 'print-preview:printer-shared'
        },
        // Shared cloud printer without stickied icon
        {
          id: 'ID3',
          origin: print_preview.DestinationOrigin.COOKIES,
          account: account,
          capabilities: null,
          displayName: 'Three',
          extensionId: '',
          extensionName: ''
        },
      ];

      document.body.appendChild(destinationSelect);
    });

    function compareIcon(selectEl, expectedIcon) {
      const icon = selectEl.style['background-image'].replace(/ /gi, '');
      const expected = getSelectDropdownBackground(
          destinationSelect.meta_.byKey('print-preview'), expectedIcon,
          destinationSelect);
      assertEquals(expected, icon);
    }

    test(assert(TestNames.ChangeIcon), function() {
      const destination = new print_preview.Destination(
          'ID1', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'One',
          print_preview.DestinationConnectionStatus.ONLINE);
      destinationSelect.destination = destination;
      destinationSelect.updateDestination();
      const selectEl = destinationSelect.$$('.md-select');
      compareIcon(selectEl, 'print');
      const driveId = print_preview.Destination.GooglePromotedId.DOCS;
      const cookieOrigin = print_preview.DestinationOrigin.COOKIES;

      return print_preview_test_utils
          .selectOption(
              destinationSelect, `${driveId}/${cookieOrigin}/${account}`)
          .then(() => {
            // Icon updates early based on the ID.
            compareIcon(selectEl, 'save-to-drive');

            // Update the destination.
            destinationSelect.destination =
                print_preview_test_utils.getGoogleDriveDestination(account);

            // Still Save to Drive icon.
            compareIcon(selectEl, 'save-to-drive');

            // Select a destination that has a sticky icon value.
            return print_preview_test_utils.selectOption(
                destinationSelect, `ID2/${cookieOrigin}/${account}`);
          })
          .then(() => {
            // Should already be updated.
            compareIcon(selectEl, 'printer-shared');

            // Update destination.
            destinationSelect.destination = new print_preview.Destination(
                'ID2', print_preview.DestinationType.GOOGLE,
                print_preview.DestinationOrigin.COOKIES, 'Two',
                print_preview.DestinationConnectionStatus.ONLINE,
                {account: account});
            compareIcon(selectEl, 'printer-shared');

            // Select a destination that doesn't have a sticky icon value.
            return print_preview_test_utils.selectOption(
                destinationSelect, `ID3/${cookieOrigin}/${account}`);
          })
          .then(() => {
            // Falls back to normal printer icon.
            compareIcon(selectEl, 'print');

            // Update destination.
            destinationSelect.destination = new print_preview.Destination(
                'ID3', print_preview.DestinationType.GOOGLE,
                print_preview.DestinationOrigin.COOKIES, 'Three',
                print_preview.DestinationConnectionStatus.ONLINE,
                {account: account});

            // Icon updates based on full destination information.
            compareIcon(selectEl, 'printer-shared');
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
