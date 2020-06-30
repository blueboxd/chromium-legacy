// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import 'chrome://nearby/nearby_progress.js';

import {assertEquals} from '../chai_assert.js';

suite('ProgressTest', function() {
  /** @type {!NearbyProgressElement} */
  let progressElement;

  setup(function() {
    progressElement = /** @type {!NearbyProgressElement} */ (
        document.createElement('nearby-progress'));
    document.body.appendChild(progressElement);
  });

  teardown(function() {
    progressElement.remove();
  });

  test('renders component', function() {
    assertEquals('NEARBY-PROGRESS', progressElement.tagName);
  });

  test('renders deviceName', function() {
    const deviceName = 'Device Name';
    progressElement.setAttribute('device-name', deviceName);

    const renderedDeviceName = progressElement.$$('#device-name').textContent;
    assertEquals(deviceName, renderedDeviceName);
  });
});
