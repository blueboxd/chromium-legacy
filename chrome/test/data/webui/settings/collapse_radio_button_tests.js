// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('CrCollapseRadioButton', function() {
  /** @type {SettingsCollapseRadioButtonElement} */
  let collapseRadioButton;

  setup(function() {
    PolymerTest.clearBody();
    collapseRadioButton =
        document.createElement('settings-collapse-radio-button');
    document.body.appendChild(collapseRadioButton);
    Polymer.dom.flush();
  });

  test('openOnSelection', function() {
    const collapse = collapseRadioButton.$$('iron-collapse');
    collapseRadioButton.checked = false;
    Polymer.dom.flush();
    assertFalse(collapse.opened);
    collapseRadioButton.checked = true;
    Polymer.dom.flush();
    assertTrue(collapse.opened);
  });

  test('closeOnDeselect', function() {
    const collapse = collapseRadioButton.$$('iron-collapse');
    collapseRadioButton.checked = true;
    Polymer.dom.flush();
    assertTrue(collapse.opened);
    collapseRadioButton.checked = false;
    Polymer.dom.flush();
    assertFalse(collapse.opened);
  });

  // When the button is not selected clicking the expand icon should still
  // open the iron collapse.
  test('openOnExpandHit', function() {
    const collapse = collapseRadioButton.$$('iron-collapse');
    collapseRadioButton.checked = false;
    Polymer.dom.flush();
    assertFalse(collapse.opened);
    collapseRadioButton.$$('cr-expand-button').click();
    Polymer.dom.flush();
    assertTrue(collapse.opened);
  });

  // When the button is selected clicking the expand icon should still close
  // the iron collapse.
  test('closeOnExpandHitWhenSelected', function() {
    const collapse = collapseRadioButton.$$('iron-collapse');
    collapseRadioButton.checked = true;
    Polymer.dom.flush();
    assertTrue(collapse.opened);
    collapseRadioButton.$$('cr-expand-button').click();
    Polymer.dom.flush();
    assertFalse(collapse.opened);
  });

  test('expansionHiddenWhenNoCollapseSet', function() {
    assertTrue(
        test_util.isChildVisible(collapseRadioButton, 'cr-expand-button'));
    assertTrue(test_util.isChildVisible(collapseRadioButton, '.separator'));

    collapseRadioButton.noCollapse = true;
    Polymer.dom.flush();
    assertFalse(
        test_util.isChildVisible(collapseRadioButton, 'cr-expand-button'));
    assertFalse(test_util.isChildVisible(collapseRadioButton, '.separator'));
  });

  test('openOnExpandHitWhenDisabled', function() {
    collapseRadioButton.checked = false;
    collapseRadioButton.disabled = true;
    const collapse = collapseRadioButton.$$('iron-collapse');

    Polymer.dom.flush();
    assertFalse(collapse.opened);
    collapseRadioButton.$$('cr-expand-button').click();

    Polymer.dom.flush();
    assertTrue(collapse.opened);
  });

  test('displayPolicyIndicator', function() {
    assertFalse(
        test_util.isChildVisible(collapseRadioButton, '#policyIndicator'));
    assertEquals(
        collapseRadioButton.policyIndicatorType, CrPolicyIndicatorType.NONE);

    collapseRadioButton.policyIndicatorType =
        CrPolicyIndicatorType.DEVICE_POLICY;
    Polymer.dom.flush();
    assertTrue(
        test_util.isChildVisible(collapseRadioButton, '#policyIndicator'));
  });
});
