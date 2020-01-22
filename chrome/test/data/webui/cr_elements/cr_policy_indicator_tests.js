// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
// #import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';
// #import {isChromeOS} from 'chrome://resources/js/cr.m.js';
// #import 'chrome://test/cr_elements/cr_policy_strings.js';
// clang-format on

/** @fileoverview Suite of tests for cr-policy-indicator. */
suite('CrPolicyIndicator', function() {
  /** @type {!CrPolicyIndicatorElement|undefined} */
  let indicator;

  /** @type {!CrTooltipIconElement|undefined} */
  let icon;

  setup(function() {
    PolymerTest.clearBody();

    indicator = document.createElement('cr-policy-indicator');
    document.body.appendChild(indicator);
    icon = indicator.$$('cr-tooltip-icon');
  });

  teardown(function() {
    PolymerTest.clearBody();  // crbug.com/680169
  });

  test('none', function() {
    assertTrue(icon.hidden);
  });

  test('indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.USER_POLICY;

    assertFalse(icon.hidden);
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals('policy', icon.tooltipText);

    if (cr.isChromeOS) {
      indicator.indicatorType = CrPolicyIndicatorType.OWNER;
      indicator.indicatorSourceName = 'foo@example.com';

      assertEquals('cr:person', icon.iconClass);
      assertEquals('owner: foo@example.com', icon.tooltipText);
    }

    indicator.indicatorType = CrPolicyIndicatorType.PARENT;

    assertFalse(icon.hidden);
    assertEquals('cr20:kite', icon.iconClass);
    assertEquals('parent', icon.tooltipText);

    indicator.indicatorType = CrPolicyIndicatorType.CHILD_RESTRICTION;

    assertFalse(icon.hidden);
    assertEquals('cr20:kite', icon.iconClass);
    assertEquals('Restricted for child', icon.tooltipText);
  });
});
