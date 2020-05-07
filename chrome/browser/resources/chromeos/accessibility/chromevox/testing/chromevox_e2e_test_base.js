// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['common.js', 'callback_helper.js']);

/**
 * Base test fixture for ChromeVox end to end tests.
 * These tests run against production ChromeVox inside of the extension's
 * background page context.
 */
ChromeVoxE2ETest = class extends testing.Test {
  constructor() {
    super();
    this.callbackHelper_ = new CallbackHelper(this);
  }

  /** @override */
  testGenCppIncludes() {
    GEN(`
  #include "ash/accessibility/accessibility_delegate.h"
  #include "ash/shell.h"
  #include "base/bind.h"
  #include "base/callback.h"
  #include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
  #include "chrome/browser/chromeos/accessibility/speech_monitor.h"
  #include "chrome/common/extensions/extension_constants.h"
  #include "content/public/test/browser_test.h"
  #include "extensions/common/extension_l10n_util.h"
      `);
  }

  /** @override */
  testGenPreamble() {
    GEN(`

    // SpeechMonitor swaps in a custom TtsPlatform to mock out events and
    // voices. Do this for the current call stack to catch deferred load.
    chromeos::SpeechMonitor speech_monitor;

    auto allow = extension_l10n_util::AllowGzippedMessagesAllowedForTest();
    base::Closure load_cb =
        base::Bind(&chromeos::AccessibilityManager::EnableSpokenFeedback,
            base::Unretained(chromeos::AccessibilityManager::Get()),
            true);
    WaitForExtension(extension_misc::kChromeVoxExtensionId, load_cb);
      `);
  }

  /**
   * Launch a new tab, wait until tab status complete, then run callback.
   * @param {function() : void} doc Snippet wrapped inside of a function.
   * @param {function()} callback Called once the document is ready.
   */
  runWithLoadedTab(doc, callback) {
    this.launchNewTabWithDoc(doc, function(tab) {
      chrome.tabs.onUpdated.addListener(function(tabId, changeInfo) {
        if (tabId == tab.id && changeInfo.status == 'complete') {
          callback(tabId);
        }
      });
    });
  }

  /**
   * Launches the given document in a new tab.
   * @param {function() : void} doc Snippet wrapped inside of a function.
   * @param {function(url: string)} opt_callback Called once the
   *     document is created.
   */
  runWithTab(doc, opt_callback) {
    const url = TestUtils.createUrlForDoc(doc);
    const createParams = {active: true, url};
    chrome.tabs.create(createParams, function(tab) {
      if (opt_callback) {
        opt_callback(tab.url);
      }
    });
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
};

/** @override */
ChromeVoxE2ETest.prototype.isAsync = true;
/** @override */
ChromeVoxE2ETest.prototype.runAccessibilityChecks = false;
