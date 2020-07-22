// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import 'chrome://nearby/app.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

suite('ShareAppTest', function() {
  /** @type {!NearbyShareAppElement} */
  let shareAppElement;

  /** @param {!string} page Page to check if it is active. */
  function isPageActive(page) {
    return shareAppElement.$$(`nearby-${page}-page`)
        .classList.contains('active');
  }

  setup(function() {
    shareAppElement = /** @type {!NearbyShareAppElement} */ (
        document.createElement('nearby-share-app'));
    document.body.appendChild(shareAppElement);
  });

  teardown(function() {
    shareAppElement.remove();
  });

  test('renders component', function() {
    assertEquals('NEARBY-SHARE-APP', shareAppElement.tagName);
  });

  test('renders nearby-discovery-page by default', function() {
    assertTrue(isPageActive('discovery'));
  });

  test('changes page on event', function() {
    // Discovery page should be active by default, other pages should not.
    assertTrue(isPageActive('discovery'));
    assertFalse(isPageActive('onboarding'));

    shareAppElement.fire('change-page', {page: 'onboarding'});

    // Onboarding page should now be active, other pages should not.
    assertTrue(isPageActive('onboarding'));
    assertFalse(isPageActive('discovery'));
  });
});
