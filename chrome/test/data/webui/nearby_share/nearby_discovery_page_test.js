// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://nearby/nearby_discovery_page.js';

import {setDiscoveryManagerForTesting} from 'chrome://nearby/discovery_manager.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

import {FakeConfirmationManagerRemote, FakeDiscoveryManagerRemote} from './fake_mojo_interfaces.js';

suite('DiscoveryPageTest', function() {
  /** @type {!NearbyDiscoveryPageElement} */
  let discoveryPageElement;

  /** @type {!FakeDiscoveryManagerRemote} */
  let discoveryManager;

  /** @type {!number} Next device id to be used. */
  let nextId = 0;

  /**
   * Get the list of device names that are currently shown.
   * @return {!Array<string>}
   */
  function getDeviceNames() {
    /** @type {!Array<string>} */
    const deviceNames = [];
    for (const device of discoveryPageElement.$$('#device-list').children) {
      if (device.is === 'nearby-device') {
        deviceNames.push(device.$$('#name').textContent);
      }
    }
    return deviceNames;
  }

  /**
   * @param {!string} name Device name
   * @return {!nearbyShare.mojom.ShareTarget}
   */
  function createShareTarget(name) {
    return {
      id: {high: 0, low: nextId++},
      name,
      type: nearbyShare.mojom.ShareTargetType.kPhone,
    };
  }

  /**
   * Creates a share target and sends it to the WebUI.
   * @return {!Promise<nearbyShare.mojom.ShareTarget>}
   */
  async function setupShareTarget() {
    /** @type {!nearbyShare.mojom.ShareTargetListenerRemote} */
    const listener = await discoveryManager.whenCalled('startDiscovery');
    const shareTarget = createShareTarget('Device Name');
    listener.onShareTargetDiscovered(shareTarget);
    await listener.$.flushForTesting();
    return shareTarget;
  }

  setup(function() {
    discoveryManager = new FakeDiscoveryManagerRemote();
    setDiscoveryManagerForTesting(discoveryManager);
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
    const created = await setupShareTarget();
    discoveryPageElement.$$('#next-button').click();
    const selectedId = await discoveryManager.whenCalled('selectShareTarget');
    assertEquals(created.id.high, selectedId.high);
    assertEquals(created.id.low, selectedId.low);
  });

  test('selects share target with error', async function() {
    await setupShareTarget();
    discoveryManager.selectShareTargetResult.result =
        nearbyShare.mojom.SelectShareTargetResult.kError;

    discoveryPageElement.$$('#next-button').click();
    await discoveryManager.whenCalled('selectShareTarget');
  });

  test('selects share target with confirmation', async function() {
    await setupShareTarget();
    discoveryManager.selectShareTargetResult.token = 'test token';
    discoveryManager.selectShareTargetResult.confirmationManager =
        new FakeConfirmationManagerRemote();

    let eventDetail = null;
    discoveryPageElement.addEventListener('change-page', (event) => {
      eventDetail = event.detail;
    });

    discoveryPageElement.$$('#next-button').click();

    await discoveryManager.whenCalled('selectShareTarget');
    assertEquals('confirmation', eventDetail.page);
  });

  test('starts discovery', async function() {
    await discoveryManager.whenCalled('startDiscovery');
  });

  test('shows newly discovered device', async function() {
    /** @type {!nearbyShare.mojom.ShareTargetListenerRemote} */
    const listener = await discoveryManager.whenCalled('startDiscovery');
    const deviceName = 'Device Name';

    listener.onShareTargetDiscovered(createShareTarget(deviceName));
    await listener.$.flushForTesting();

    assertTrue(getDeviceNames().includes(deviceName));
  });

  test('shows multiple discovered devices', async function() {
    /** @type {!nearbyShare.mojom.ShareTargetListenerRemote} */
    const listener = await discoveryManager.whenCalled('startDiscovery');
    const deviceName1 = 'Device Name 1';
    const deviceName2 = 'Device Name 2';

    listener.onShareTargetDiscovered(createShareTarget(deviceName1));
    listener.onShareTargetDiscovered(createShareTarget(deviceName2));
    await listener.$.flushForTesting();

    assertTrue(getDeviceNames().includes(deviceName1));
    assertTrue(getDeviceNames().includes(deviceName2));
  });

  test('removes lost device', async function() {
    /** @type {!nearbyShare.mojom.ShareTargetListenerRemote} */
    const listener = await discoveryManager.whenCalled('startDiscovery');
    const deviceName = 'Device Name';
    const shareTarget = createShareTarget(deviceName);

    listener.onShareTargetDiscovered(shareTarget);
    listener.onShareTargetLost(shareTarget);
    await listener.$.flushForTesting();

    assertFalse(getDeviceNames().includes(deviceName));
  });

  test('replaces existing device', async function() {
    /** @type {!nearbyShare.mojom.ShareTargetListenerRemote} */
    const listener = await discoveryManager.whenCalled('startDiscovery');
    const deviceName = 'Device Name';
    const shareTarget = createShareTarget(deviceName);

    listener.onShareTargetDiscovered(shareTarget);
    await listener.$.flushForTesting();

    const expectedDeviceCount = getDeviceNames().length;

    listener.onShareTargetDiscovered(shareTarget);
    await listener.$.flushForTesting();

    assertEquals(expectedDeviceCount, getDeviceNames().length);
  });
});
