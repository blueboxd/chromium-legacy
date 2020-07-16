// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import 'chrome://nearby/nearby_device.js';

import {assertEquals} from '../chai_assert.js';

suite('DeviceTest', function() {
  /** @type {!NearbyDeviceElement} */
  let deviceElement;

  setup(function() {
    deviceElement = /** @type {!NearbyDeviceElement} */ (
        document.createElement('nearby-device'));
    document.body.appendChild(deviceElement);
  });

  teardown(function() {
    deviceElement.remove();
  });

  test('renders component', function() {
    assertEquals('NEARBY-DEVICE', deviceElement.tagName);
  });

  test('renders name', function() {
    const name = 'Name';
    deviceElement.setAttribute('name', name);

    const renderedName = deviceElement.$$('#name').textContent;
    assertEquals(name, renderedName);
  });
});
