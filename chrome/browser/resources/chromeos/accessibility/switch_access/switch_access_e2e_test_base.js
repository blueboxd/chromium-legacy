// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
  '../common/testing/assert_additions.js', '../common/testing/e2e_test_base.js'
]);

/** Base class for browser tests for Switch Access. */
SwitchAccessE2ETest = class extends E2ETestBase {
  /** @override */
  testGenCppIncludes() {
    GEN(`
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ash/keyboard/ui/keyboard_util.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    GEN(`
  base::Closure load_cb =
      base::Bind(&chromeos::AccessibilityManager::SetSwitchAccessEnabled,
          base::Unretained(chromeos::AccessibilityManager::Get()),
          true);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilitySwitchAccess);
  chromeos::AccessibilityManager::Get()->SetSwitchAccessEnabled(true);
  WaitForExtension(extension_misc::kSwitchAccessExtensionId, load_cb);
    `);
  }

  /**
   * @param {function(AutomationNode): boolean} predicate A predicate that
   *     uniquely specifies one automation node.
   * @param {string=} opt_nodeString A string specifying what node was being
   *     looked for.
   * @return {!AutomationNode}
   */
  findNodeMatchingPredicate(predicate, opt_nodeString) {
    const nodeString = opt_nodeString || 'node matching the predicate';
    const treeWalker = new AutomationTreeWalker(
        NavigationManager.desktopNode, constants.Dir.FORWARD,
        {visit: predicate});
    const node = treeWalker.next().node;
    assertNotNullNorUndefined(node, 'Could not find ' + nodeString + '.');
    assertNullOrUndefined(
        treeWalker.next().node, 'Found more than one ' + nodeString + '.');
    return node;
  }

  /**
   * @param {string} id The HTML id of an element.
   * @return {!AutomationNode}
   */
  findNodeById(id) {
    const predicate = (node) => node.htmlAttributes.id === id;
    const nodeString = 'node with id "' + id + '"';
    return this.findNodeMatchingPredicate(predicate, nodeString);
  }

  /**
   * @param {string} name The name of the node within the automation tree.
   * @param {string} role The node's role.
   * @return {!AutomationNode}
   */
  findNodeByNameAndRole(name, role) {
    const predicate = (node) => node.name === name && node.role === role;
    const nodeString = 'node with name "' + name + '" and role ' + role;
    return this.findNodeMatchingPredicate(predicate, nodeString);
  }

  /**
   * @param {function(): boolean} predicate The condition under which the
   *     callback should be fired.
   * @param {function()} callback
   */
  waitForPredicate(predicate, callback) {
    if (predicate()) {
      callback();
      return;
    }
    const listener = () => {
      if (predicate()) {
        NavigationManager.desktopNode.removeEventListener(
            'childrenChanged', listener, false /* capture */);
        callback();
      }
    };
    NavigationManager.desktopNode.addEventListener(
        'childrenChanged', listener, false /* capture */);
  }
};
