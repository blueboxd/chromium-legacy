// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../switch_access_e2e_test_base.js']);

/**
 * @constructor
 * @extends {SwitchAccessE2ETest}
 */
function SwitchAccessTabNodeTest() {
  SwitchAccessE2ETest.call(this);
}

SwitchAccessTabNodeTest.prototype = {
  __proto__: SwitchAccessE2ETest.prototype,

  /** @override */
  setUp() {
    MenuManager.initialize();
  },
};

TEST_F('SwitchAccessTabNodeTest', 'FindCloseButton', function() {
  this.runWithLoadedTree('', (desktop) => {
    const tab = desktop.find({role: chrome.automation.RoleType.TAB});

    // To find the close button, Switch Access relies on it being the only
    // button within a tab.
    let buttonCount = 0;
    for (const child of tab.children) {
      if (child.role === chrome.automation.RoleType.BUTTON) {
        buttonCount++;
      }
    }
    assertEquals(buttonCount, 1);
  });
});

TEST_F('SwitchAccessTabNodeTest', 'Construction', function() {
  this.runWithLoadedTree('', (desktop) => {
    NavigationManager.instance.moveTo_(
        desktop.find({role: chrome.automation.RoleType.TAB}));

    const tab = NavigationManager.instance.node_;
    assertEquals(
        chrome.automation.RoleType.TAB, tab.role, 'Tab node is not a tab');
    assertTrue(tab.isGroup(), 'Tab node should be a group');
    assertEquals(
        0, tab.actions.length, 'Tab as a group should not have actions');

    NavigationManager.instance.selectCurrentNode();

    const tabAsRoot = NavigationManager.instance.group_;
    assertTrue(
        RectHelper.areEqual(tab.location, tabAsRoot.location),
        'Tab location should not change when treated as root');
    assertEquals(
        3, tabAsRoot.children.length, 'Tab as root should have 3 children');

    const tabToSelect = NavigationManager.instance.node_;
    assertEquals(
        chrome.automation.RoleType.TAB, tabToSelect.role,
        'Tab node to select is not a tab');
    assertFalse(
        tabToSelect.isGroup(), 'Tab node to select should not be a group');
    assertTrue(
        tabToSelect.hasAction(SAConstants.MenuAction.SELECT),
        'Tab as a group should have a SELECT action');
    assertFalse(
        RectHelper.areEqual(tabAsRoot.location, tabToSelect.location),
        'Tab node to select should not have the same location as tab as root');
    assertEquals(
        null, tabToSelect.asRootNode(),
        'Tab node to select should not be a root node');

    NavigationManager.moveForward();

    const close = NavigationManager.instance.node_;
    assertEquals(
        chrome.automation.RoleType.BUTTON, close.role,
        'Close button is not a button');
    assertFalse(close.isGroup(), 'Close button should not be a group');
    assertTrue(
        close.hasAction(SAConstants.MenuAction.SELECT),
        'Close button should have a SELECT action');
    assertFalse(
        RectHelper.areEqual(tabAsRoot.location, close.location),
        'Close button should not have the same location as tab as root');
    const overlap =
        RectHelper.intersection(tabToSelect.location, close.location);
    assertTrue(
        RectHelper.areEqual(RectHelper.ZERO_RECT, overlap),
        'Close button and tab node to select should not overlap');

    BackButtonNode
        .locationForTesting = {top: 10, left: 10, width: 10, height: 10};
    NavigationManager.moveForward();
    assertTrue(
        NavigationManager.instance.node_ instanceof BackButtonNode,
        'Third node should be a BackButtonNode');
  });
});
