// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import 'chrome://nearby/nearby_confirmation_page.js';

import {assertEquals, assertTrue} from '../chai_assert.js';
import {FakeConfirmationManagerRemote} from './fake_mojo_interfaces.js';

suite('ConfirmatonPageTest', function() {
  /** @type {!NearbyConfirmationPageElement} */
  let confirmationPageElement;

  setup(function() {
    confirmationPageElement = /** @type {!NearbyConfirmationPageElement} */ (
        document.createElement('nearby-confirmation-page'));
    document.body.appendChild(confirmationPageElement);
  });

  teardown(function() {
    confirmationPageElement.remove();
  });

  test('renders component', function() {
    assertEquals('NEARBY-CONFIRMATION-PAGE', confirmationPageElement.tagName);
  });

  test('calls accept on click', async function() {
    const confirmationManager = new FakeConfirmationManagerRemote();
    confirmationPageElement.confirmationManager = confirmationManager;
    confirmationPageElement.$$('#accept-button').click();
    await confirmationManager.whenCalled('accept');
  });

  test('calls reject on click', async function() {
    const confirmationManager = new FakeConfirmationManagerRemote();
    confirmationPageElement.confirmationManager = confirmationManager;
    confirmationPageElement.$$('#cancel-button').click();
    await confirmationManager.whenCalled('reject');
  });

  test('renders confirmation token', function() {
    const token = 'TestToken1234';
    confirmationPageElement.confirmationToken = token;
    const renderedToken =
        confirmationPageElement.$$('#confirmation-token').textContent;
    assertTrue(renderedToken.includes(token));
  });

  test('renders share target name', function() {
    const name = 'Device Name';
    confirmationPageElement.shareTarget = {
      id: {high: 0, low: 0},
      name,
      type: nearbyShare.mojom.ShareTargetType.kPhone,
    };
    const renderedName = confirmationPageElement.$$('nearby-progress')
                             .$$('#device-name')
                             .textContent;
    assertEquals(name, renderedName);
  });
});
