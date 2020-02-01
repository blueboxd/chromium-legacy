// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['chromevox_e2e_test_base.js']);

/**
 * Base test fixture for ChromeVox Next end to end tests.
 * These tests are identical to ChromeVoxE2ETests except for performing the
 * necessary setup to run ChromeVox Next.
 */
ChromeVoxNextE2ETest = class extends ChromeVoxE2ETest {
  constructor() {
    super();

    if (this.runtimeDeps.length > 0) {
      chrome.extension.getViews().forEach(function(w) {
        this.runtimeDeps.forEach(function(dep) {
          if (w[dep]) {
            window[dep] = w[dep];
          }
        }.bind(this));
      }.bind(this));
    }

    // For tests, enable announcement of events we trigger via automation.
    DesktopAutomationHandler.announceActions = true;

    this.originalOutputContextValues_ = {};
    for (const role in Output.ROLE_INFO_) {
      this.originalOutputContextValues_[role] =
          Output.ROLE_INFO_[role]['outputContextFirst'];
    }
  }

  /**
   * Dependencies defined on a background window other than this one.
   * @type {!Array<string>}
   */
  get runtimeDeps() {
    return [];
  }

  /**
   * Gets the desktop from the automation API and Launches a new tab with
   * the given document, and runs |callback| when a load complete fires.
   * Arranges to call |testDone()| after |callback| returns.
   * NOTE: Callbacks creatd instide |opt_callback| must be wrapped with
   * |this.newCallback| if passed to asynchonous calls.  Otherwise, the test
   * will be finished prematurely.
   * @param {function() : void} doc Snippet wrapped inside of a function.
   * @param {function(chrome.automation.AutomationNode)} callback
   *     Called once the document is ready.
   * @param {string=} opt_url Optional url to wait for. Defaults to undefined.
   */
  runWithLoadedTree(doc, callback, opt_url) {
    callback = this.newCallback(callback);
    chrome.automation.getDesktop(function(r) {
      const url = opt_url || TestUtils.createUrlForDoc(doc);
      const listener = function(evt) {
        if (evt.target.root.url != url) {
          return;
        }

        if (!evt.target.root.docLoaded) {
          return;
        }

        r.removeEventListener('focus', listener, true);
        r.removeEventListener('loadComplete', listener, true);
        CommandHandler.onCommand('nextObject');
        callback && callback(evt.target);
        callback = null;
      };
      r.addEventListener('focus', listener, true);
      r.addEventListener('loadComplete', listener, true);
      const createParams = {active: true, url};
      chrome.tabs.create(createParams);
    }.bind(this));
  }

  listenOnce(node, eventType, callback, capture) {
    const innerCallback = this.newCallback(function() {
      node.removeEventListener(eventType, innerCallback, capture);
      callback.apply(this, arguments);
    });
    node.addEventListener(eventType, innerCallback, capture);
  }

  /**
   * Forces output to place context utterances at the end of output. This eases
   * rebaselining when changing context ordering for a specific role.
   */
  forceContextualLastOutput() {
    for (const role in Output.ROLE_INFO_) {
      Output.ROLE_INFO_[role]['outputContextFirst'] = undefined;
    }
  }

  /**
   * Forces output to place context utterances at the beginning of output.
   */
  forceContextualFirstOutput() {
    for (const role in Output.ROLE_INFO_) {
      Output.ROLE_INFO_[role]['outputContextFirst'] = true;
    }
  }

  /** Resets contextual output values to their defaults. */
  resetContextualOutput() {
    for (const role in Output.ROLE_INFO_) {
      Output.ROLE_INFO_[role]['outputContextFirst'] =
          this.originalOutputContextValues_[role];
    }
  }
};
