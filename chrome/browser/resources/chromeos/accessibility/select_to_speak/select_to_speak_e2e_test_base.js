// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../chromevox/testing/callback_helper.js']);

/**
 * Base class for browser tests for select-to-speak.
 */
SelectToSpeakE2ETest = class extends testing.Test {
  constructor() {
    super();
    this.callbackHelper_ = new CallbackHelper(this);
  }

  /** @override */
  testGenCppIncludes() {
    GEN(`
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_test.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    GEN(`
    //keyboard::SetRequestedKeyboardState(keyboard::KEYBOARD_STATE_ENABLED);
    //ash::Shell::Get()->CreateKeyboard();
    base::Closure load_cb =
        base::Bind(&chromeos::AccessibilityManager::SetSelectToSpeakEnabled,
            base::Unretained(chromeos::AccessibilityManager::Get()),
            true);
    chromeos::AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
    WaitForExtension(extension_misc::kSelectToSpeakExtensionId, load_cb);
      `);
  }

  /**
   * Creates a callback that optionally calls {@code opt_callback} when
   * called.  If this method is called one or more times, then
   * {@code testDone()} will be called when all callbacks have been called.
   * @param {Function=} opt_callback Wrapped callback that will have its this
   *        reference bound to the test fixture.
   * @return {Function}
   */
  newCallback(opt_callback) {
    return this.callbackHelper_.wrap(opt_callback);
  }

  /**
   * Asserts that two strings are equal, collapsing repeated spaces and
   * removing leading / trailing whitespace.
   * @param {string} first The first string to compare.
   * @param {string} second The second string to compare.
   */
  assertEqualsCollapseWhitespace(first, second) {
    assertEquals(
        first.replace(/\s+/g, ' ').replace(/^\s/, '').replace(/\s$/, ''),
        second.replace(/\s+/g, ' ').replace(/^\s/, '').replace(/\s$/, ''));
  }

  /**
   * From chromevox_next_e2e_test_base.js
   * Gets the desktop from the automation API and Launches a new tab with
   * the given document, and runs |callback| with the desktop when a load
   * complete fires on the created tab.
   * Arranges to call |testDone()| after |callback| returns.
   * NOTE: Callbacks created inside |callback| must be wrapped with
   * |this.newCallback| if passed to asynchonous calls.  Otherwise, the test
   * will be finished prematurely.
   * @param {string} url Url to load and wait for.
   * @param {function(chrome.automation.AutomationNode)} callback Called with
   *     the desktop node once the document is ready.
   */
  runWithLoadedTree(url, callback) {
    callback = this.newCallback(callback);
    chrome.automation.getDesktop(function(desktopRootNode) {
      var createParams = {active: true, url};
      chrome.tabs.create(createParams, function(unused_tab) {
        chrome.automation.getTree(function(returnedRootNode) {
          const rootNode = returnedRootNode;
          if (rootNode.docLoaded) {
            callback && callback(desktopRootNode);
            callback = null;
            return;
          }
          rootNode.addEventListener('loadComplete', function(evt) {
            if (evt.target.root.url != url) {
              return;
            }
            callback && callback(desktopRootNode);
            callback = null;
          });
        });
      });
    }.bind(this));
  }

  /**
   * Helper function to find a staticText node from a root
   * @param {AutomationNode} root The root node to search through
   * @param {string} text The text to search for
   * @return {AutomationNode} The found text node, or null if none is found.
   */
  findTextNode(root, text) {
    return root.find({role: 'staticText', attributes: {name: text}});
  }
};

/**
 * @override
 * No UI in the background context.
 */
SelectToSpeakE2ETest.prototype.runAccessibilityChecks = false;

/** @override */
SelectToSpeakE2ETest.prototype.isAsync = true;

/** @override */
SelectToSpeakE2ETest.prototype.browsePreload = null;
