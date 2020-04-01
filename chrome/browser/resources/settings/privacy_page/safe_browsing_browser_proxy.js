// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles Safe Browsing settings updates */

// clang-format on
// #import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// #import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
// #import {ManagedState} from '../site_settings/site_settings_prefs_browser_proxy.m.js';
// clang-format off

cr.define('settings', function() {
  /**
   * Represents the management state of the Safe Browsing radio buttons.
   * @typedef {{enhanced: !ManagedState,
   *            standard: !ManagedState,
   *            disabled: !ManagedState }}
   */
  /* #export */ let SafeBrowsingRadioManagedState;

  /** @interface */
  /* #export */ class SafeBrowsingBrowserProxy {
    /**
     * Gets the current Safe Browsing radio controls managed state.
     * @return {!Promise<!settings.SafeBrowsingRadioManagedState>}
     */
    getSafeBrowsingRadioManagedState() {}
  }

  /** @implements {settings.SafeBrowsingBrowserProxy} */
  /* #export */ class SafeBrowsingBrowserProxyImpl {
    /** @override */
    getSafeBrowsingRadioManagedState() {
      return cr.sendWithPromise('getSafeBrowsingRadioManagedState');
    }
  }

  cr.addSingletonGetter(SafeBrowsingBrowserProxyImpl);

  // #cr_define_end
  return {
    SafeBrowsingRadioManagedState,
    SafeBrowsingBrowserProxy,
    SafeBrowsingBrowserProxyImpl,
  };
});
