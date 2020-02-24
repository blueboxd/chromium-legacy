// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../switch_access_e2e_test_base.js']);

/**
 * @constructor
 * @extends {SwitchAccessE2ETest}
 */
function SwitchAccessNodeWrapperTest() {
  SwitchAccessE2ETest.call(this);
}

SwitchAccessNodeWrapperTest.prototype = {
  __proto__: SwitchAccessE2ETest.prototype,
};

TEST_F('SwitchAccessNodeWrapperTest', 'AsRootNode', function() {
  const website = `<div aria-label="outer">
                     <div aria-label="inner">
                       <input type="range">
                       <button></button>
                     </div>
                     <button></button>
                   </div>`;
  this.runWithLoadedTree(website, (desktop) => {
    const slider = desktop.find({role: chrome.automation.RoleType.SLIDER});
    const inner = slider.parent;
    assertNotEquals(undefined, inner, 'Could not find inner group');
    const outer = inner.parent;
    assertNotEquals(undefined, outer, 'Could not find outer group');

    const outerRootNode = RootNodeWrapper.buildTree(outer, null);
    const innerNode = outerRootNode.firstChild;
    assertTrue(innerNode.isGroup(), 'Inner group node is not a group');

    const innerRootNode = innerNode.asRootNode();
    assertEquals(3, innerRootNode.children.length, 'Expected 3 children');
    const sliderNode = innerRootNode.firstChild;
    assertEquals(
        chrome.automation.RoleType.SLIDER, sliderNode.role,
        'First child should be a slider');
    assertEquals(
        chrome.automation.RoleType.BUTTON, sliderNode.next.role,
        'Second child should be a button');
    assertTrue(
        innerRootNode.lastChild instanceof BackButtonNode,
        'Final child should be the back button');
  });
});

TEST_F('SwitchAccessNodeWrapperTest', 'Equals', function() {
  this.runWithLoadedTree('', (desktop) => {
    const desktopNode = DesktopNode.build(desktop);

    let childGroup = desktopNode.firstChild;
    let i = 0;
    while (!childGroup.isGroup() && i < desktopNode.children.length) {
      childGroup = childGroup.next;
      i++;
    }
    childGroup = childGroup.asRootNode();

    assertFalse(desktopNode.equals(), 'Root node equals nothing');
    assertFalse(
        desktopNode.equals(new SARootNode()),
        'Different type root nodes are equal');
    assertFalse(
        new SARootNode().equals(desktopNode),
        'Equals is not symmetric? Different types of root are equal');
    assertFalse(
        desktopNode.equals(childGroup),
        'Groups with different children are equal');
    assertFalse(
        childGroup.equals(desktopNode),
        'Equals is not symmetric? Groups with different children are equal');

    assertTrue(
        desktopNode.equals(desktopNode),
        'Equals is not reflexive? (root node)');
    const desktopCopy = DesktopNode.build(desktop);
    assertTrue(
        desktopNode.equals(desktopCopy), 'Two desktop roots are not equal');
    assertTrue(
        desktopCopy.equals(desktopNode),
        'Equals is not symmetric? Two desktop roots aren\'t equal');

    const wrappedNode = desktopNode.firstChild;
    assertTrue(
        wrappedNode instanceof NodeWrapper,
        'Child node is not of type NodeWrapper');
    assertGT(desktopNode.children.length, 1, 'Desktop root has only 1 child');

    assertFalse(wrappedNode.equals(), 'Child NodeWrapper equals nothing');
    assertFalse(
        wrappedNode.equals(new BackButtonNode()),
        'Child NodeWrapper equals a BackButtonNode');
    assertFalse(
        new BackButtonNode().equals(wrappedNode),
        'Equals is not symmetric? NodeWrapper equals a BackButtonNode');
    assertFalse(
        wrappedNode.equals(desktopNode.lastChild),
        'Children with different base nodes are equal');
    assertFalse(
        desktopNode.lastChild.equals(wrappedNode),
        'Equals is not symmetric? Nodes with different base nodes are equal');

    const equivalentWrappedNode =
        NodeWrapper.create(wrappedNode.baseNode_, desktopNode);
    assertTrue(
        wrappedNode.equals(wrappedNode),
        'Equals is not reflexive? (child node)');
    assertTrue(
        wrappedNode.equals(equivalentWrappedNode),
        'Two nodes with the same base node are not equal');
    assertTrue(
        equivalentWrappedNode.equals(wrappedNode),
        'Equals is not symmetric? Nodes with the same base node aren\'t equal');
  });
});

TEST_F('SwitchAccessNodeWrapperTest', 'Actions', function() {
  const website = `<input type="text">
                   <button></button>
                   <input type="range" min=1 max=5 value=3>`;
  this.runWithLoadedTree(website, (desktop) => {
    const textField = NodeWrapper.create(
        desktop.find({role: chrome.automation.RoleType.TEXT_FIELD}),
        new SARootNode());

    assertEquals(
        chrome.automation.RoleType.TEXT_FIELD, textField.role,
        'Text field node is not a text field');
    assertTrue(
        textField.hasAction(SAConstants.MenuAction.OPEN_KEYBOARD),
        'Text field does not have action OPEN_KEYBOARD');
    assertTrue(
        textField.hasAction(SAConstants.MenuAction.DICTATION),
        'Text field does not have action DICTATION');
    assertFalse(
        textField.hasAction(SAConstants.MenuAction.SELECT),
        'Text field has action SELECT');

    const button = NodeWrapper.create(
        desktop.find({role: chrome.automation.RoleType.BUTTON}),
        new SARootNode());

    assertEquals(
        chrome.automation.RoleType.BUTTON, button.role,
        'Button node is not a button');
    assertTrue(
        button.hasAction(SAConstants.MenuAction.SELECT),
        'Button does not have action SELECT');
    assertFalse(
        button.hasAction(SAConstants.MenuAction.OPEN_KEYBOARD),
        'Button has action OPEN_KEYBOARD');
    assertFalse(
        button.hasAction(SAConstants.MenuAction.DICTATION),
        'Button has action DICTATION');

    const slider = NodeWrapper.create(
        desktop.find({role: chrome.automation.RoleType.SLIDER}),
        new SARootNode());

    assertEquals(
        chrome.automation.RoleType.SLIDER, slider.role,
        'Slider node is not a slider');
    assertTrue(
        slider.hasAction(SAConstants.MenuAction.INCREMENT),
        'Slider does not have action INCREMENT');
    assertTrue(
        slider.hasAction(SAConstants.MenuAction.DECREMENT),
        'Slider does not have action DECREMENT');
  });
});
