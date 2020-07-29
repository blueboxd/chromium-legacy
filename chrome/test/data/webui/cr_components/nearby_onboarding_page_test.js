// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_components/nearby_share/nearby_onboarding_page.m.js';
//
// #import {assertEquals} from '../chai_assert.js';
// clang-format on

suite('nearby-onboarding-page', function() {
  /** @type {!NearbyOnboardingPageElement} */
  let element;

  setup(function() {
    document.body.innerHTML = '';
    element = /** @type {!NearbyOnboardingPageElement} */ (
        document.createElement('nearby-onboarding-page'));
    document.body.appendChild(element);
  });

  test('Renders onboarding page', async function() {
    assertEquals('NEARBY-ONBOARDING-PAGE', element.tagName);
  });
});
