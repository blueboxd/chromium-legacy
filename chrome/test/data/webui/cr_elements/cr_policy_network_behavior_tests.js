// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for CrPolicyIndicatorBehavior. */
suite('CrPolicyNetworkBehavior', function() {
  suiteSetup(function() {
    Polymer({
      is: 'test-behavior',

      behaviors: [CrPolicyNetworkBehavior],
    });
  });

  let testBehavior;
  setup(function() {
    PolymerTest.clearBody();
    testBehavior = document.createElement('test-behavior');
    document.body.appendChild(testBehavior);
  });

  test('pod', function() {
    let property = 'foo';
    assertFalse(testBehavior.isNetworkPolicyControlled(property));
    assertFalse(testBehavior.isControlled(property));
    assertFalse(testBehavior.isExtensionControlled(property));
    assertFalse(testBehavior.isEditable(property));
    assertFalse(testBehavior.isNetworkPolicyEnforced(property));
    assertFalse(testBehavior.isNetworkPolicyRecommended(property));
  });

  test('active', function() {
    let property = {'Active': 'foo'};
    assertFalse(testBehavior.isNetworkPolicyControlled(property));
    assertFalse(testBehavior.isControlled(property));
    assertFalse(testBehavior.isExtensionControlled(property));
    assertFalse(testBehavior.isEditable(property));
    assertFalse(testBehavior.isNetworkPolicyEnforced(property));
    assertFalse(testBehavior.isNetworkPolicyRecommended(property));
  });

  test('user_recommended', function() {
    let properties = {
      'Source': 'UserPolicy',
      'a': {
        'Effective': 'UserSetting',
        'UserEditable': true,
        'UserPolicy': 'bar',
        'UserSetting': 'foo',
      }
    };
    assertTrue(testBehavior.isNetworkPolicyControlled(properties.a));
    assertTrue(testBehavior.isControlled(properties.a));
    assertFalse(testBehavior.isExtensionControlled(properties.a));
    assertTrue(testBehavior.isEditable(properties.a));
    assertFalse(testBehavior.isNetworkPolicyEnforced(properties.a));
    assertTrue(testBehavior.isNetworkPolicyRecommended(properties.a));
    assertEquals(
        CrPolicyIndicatorType.USER_POLICY,
        testBehavior.getPolicyIndicatorType_(properties, 'a'));
  });

  test('device_recommended', function() {
    let properties = {
      'Source': 'DevicePolicy',
      'a': {
        'Effective': 'DeviceSetting',
        'DeviceEditable': true,
        'DevicePolicy': 'bar',
        'DeviceSetting': 'foo',
      }
    };
    assertTrue(testBehavior.isNetworkPolicyControlled(properties.a));
    assertTrue(testBehavior.isControlled(properties.a));
    assertFalse(testBehavior.isExtensionControlled(properties.a));
    assertTrue(testBehavior.isEditable(properties.a));
    assertFalse(testBehavior.isNetworkPolicyEnforced(properties.a));
    assertTrue(testBehavior.isNetworkPolicyRecommended(properties.a));
    assertEquals(
        CrPolicyIndicatorType.DEVICE_POLICY,
        testBehavior.getPolicyIndicatorType_(properties, 'a'));
  });

  test('user_enforced', function() {
    let properties = {
      'Source': 'UserPolicy',
      'a': {
        'Effective': 'UserPolicy',
        'UserEditable': false,
        'UserPolicy': 'bar',
      }
    };
    assertTrue(testBehavior.isNetworkPolicyControlled(properties.a));
    assertTrue(testBehavior.isControlled(properties.a));
    assertFalse(testBehavior.isExtensionControlled(properties.a));
    assertFalse(testBehavior.isEditable(properties.a));
    assertTrue(testBehavior.isNetworkPolicyEnforced(properties.a));
    assertFalse(testBehavior.isNetworkPolicyRecommended(properties.a));
    assertEquals(
        CrPolicyIndicatorType.USER_POLICY,
        testBehavior.getPolicyIndicatorType_(properties, 'a'));
  });

  test('device_enforced', function() {
    let properties = {
      'Source': 'DevicePolicy',
      'a': {
        'Effective': 'DevicePolicy',
        'DeviceEditable': false,
        'DevicePolicy': 'bar',
      }
    };
    assertTrue(testBehavior.isNetworkPolicyControlled(properties.a));
    assertTrue(testBehavior.isControlled(properties.a));
    assertFalse(testBehavior.isExtensionControlled(properties.a));
    assertFalse(testBehavior.isEditable(properties.a));
    assertTrue(testBehavior.isNetworkPolicyEnforced(properties.a));
    assertFalse(testBehavior.isNetworkPolicyRecommended(properties.a));
    assertEquals(
        CrPolicyIndicatorType.DEVICE_POLICY,
        testBehavior.getPolicyIndicatorType_(properties, 'a'));
  });

  test('extension_controlled', function() {
    let properties = {
      'Source': 'UserPolicy',
      'a': {
        'Active': 'bar',
        'Effective': 'ActiveExtension',
        'UserEditable': false,
      }
    };
    assertFalse(testBehavior.isNetworkPolicyControlled(properties.a));
    assertTrue(testBehavior.isControlled(properties.a));
    assertTrue(testBehavior.isExtensionControlled(properties.a));
    assertFalse(testBehavior.isEditable(properties.a));
    assertFalse(testBehavior.isNetworkPolicyEnforced(properties.a));
    assertFalse(testBehavior.isNetworkPolicyRecommended(properties.a));

    // TODO(stevenjb): We should probably show the correct indicator for
    // extension controlled properties; fix this in the mojo code.
    assertEquals(
        CrPolicyIndicatorType.NONE,
        testBehavior.getPolicyIndicatorType_(properties, 'a'));
  });
});
