// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['assert_additions.js', 'callback_helper.js', 'doc_utils.js']);

/**
 * Base test fixture for end to end tests of accessibility component extensions.
 * These tests are intended to run inside of the extension's background page
 * context.
 */
E2ETestBase = class extends testing.Test {
  constructor() {
    super();
    this.callbackHelper_ = new CallbackHelper(this);
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
   * Gets the desktop from the automation API and Launches a new tab with
   * the given document, and runs |callback| when a load complete fires.
   * Arranges to call |testDone()| after |callback| returns.
   * NOTE: Callbacks creatd instide |opt_callback| must be wrapped with
   * |this.newCallback| if passed to asynchronous calls.  Otherwise, the test
   * will be finished prematurely.
   * @param {string|function(): string} doc An HTML snippet, optionally wrapped
   *     inside of a function.
   * @param {function(chrome.automation.AutomationNode)} callback
   *     Called once the document is ready.
   * @param {{url: (string=), returnPage: (boolean=)}}
   *     opt_params
   *           url Optional url to wait for. Defaults to undefined.
   *           returnPage True if the node for the root web area should be
   *               returned; otherwise the desktop will be returned.
   */
  runWithLoadedTree(doc, callback, opt_params = {}) {
    callback = this.newCallback(callback);
    chrome.automation.getDesktop((desktop) => {
      const url = opt_params.url || DocUtils.createUrlForDoc(doc);
      const listener = (event) => {
        if (event.target.root.url !== url || !event.target.root.docLoaded) {
          return;
        }

        desktop.removeEventListener('focus', listener, true);
        desktop.removeEventListener('loadComplete', listener, true);
        callback && callback(opt_params.returnPage ? event.target : desktop);
        callback = null;
      };
      desktop.addEventListener('focus', listener, true);
      desktop.addEventListener('loadComplete', listener, true);

      const createParams = {active: true, url};
      chrome.tabs.create(createParams);
    });
  }
};

/** @override */
E2ETestBase.prototype.isAsync = true;
/**
 * @override
 * No UI in the background context.
 */
E2ETestBase.prototype.runAccessibilityChecks = false;
