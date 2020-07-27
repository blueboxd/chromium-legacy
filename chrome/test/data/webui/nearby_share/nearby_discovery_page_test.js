// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://nearby/nearby_discovery_page.js';

import {setDiscoveryManagerForTesting} from 'chrome://nearby/discovery_manager.js';

import {assertEquals} from '../chai_assert.js';

import {FakeConfirmationManagerRemote, FakeDiscoveryManagerRemote} from './fake_mojo_interfaces.js';

suite('DiscoveryPageTest', function() {
  /** @type {!NearbyDiscoveryPageElement} */
  let discoveryPageElement;

  setup(function() {
    discoveryPageElement = /** @type {!NearbyDiscoveryPageElement} */ (
        document.createElement('nearby-discovery-page'));
    document.body.appendChild(discoveryPageElement);
  });

  teardown(function() {
    discoveryPageElement.remove();
  });

  test('renders component', function() {
    assertEquals('NEARBY-DISCOVERY-PAGE', discoveryPageElement.tagName);
  });

  test('selects share target with success', async function() {
    const manager = new FakeDiscoveryManagerRemote();
    setDiscoveryManagerForTesting(manager);

    discoveryPageElement.$$('#next-button').click();

    await manager.whenCalled('selectShareTarget');
  });

  test('selects share target with error', async function() {
    const manager = new FakeDiscoveryManagerRemote();
    manager.selectShareTargetResult.result =
        nearbyShare.mojom.SelectShareTargetResult.kError;
    setDiscoveryManagerForTesting(manager);

    discoveryPageElement.$$('#next-button').click();

    await manager.whenCalled('selectShareTarget');
  });

  test('selects share target with confirmation', async function() {
    const manager = new FakeDiscoveryManagerRemote();
    manager.selectShareTargetResult.token = 'test token';
    manager.selectShareTargetResult.confirmationManager =
        new FakeConfirmationManagerRemote();
    setDiscoveryManagerForTesting(manager);

    let eventDetail = null;
    discoveryPageElement.addEventListener('change-page', (event) => {
      eventDetail = event.detail;
    });

    discoveryPageElement.$$('#next-button').click();

    await manager.whenCalled('selectShareTarget');
    assertEquals('confirmation', eventDetail.page);
  });
});
