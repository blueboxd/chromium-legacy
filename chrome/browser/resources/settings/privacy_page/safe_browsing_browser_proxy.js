// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles Safe Browsing settings updates */

// clang-format on
import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

import {ManagedState} from '../site_settings/site_settings_prefs_browser_proxy.js';
// clang-format off

  /**
   * Represents the management state of the Safe Browsing radio buttons.
   * @typedef {{enhanced: !ManagedState,
   *            standard: !ManagedState,
   *            disabled: !ManagedState }}
   */
  export let SafeBrowsingRadioManagedState;

  /** @interface */
  export class SafeBrowsingBrowserProxy {
    /**
     * Gets the current Safe Browsing radio controls managed state.
     * @return {!Promise<!SafeBrowsingRadioManagedState>}
     */
    getSafeBrowsingRadioManagedState() {}

    /**
     * Ensures that Safe Browsing Enhanced preference is in a consistent
     * state for the currently enabled features.
     * TODO(crbug.com/1074499) Remove this logic when Enhanced protection is
     * considered stable.
     */
     validateSafeBrowsingEnhanced() {}
  }

  /** @implements {SafeBrowsingBrowserProxy} */
  export class SafeBrowsingBrowserProxyImpl {
    /** @override */
    getSafeBrowsingRadioManagedState() {
      return sendWithPromise('getSafeBrowsingRadioManagedState');
    }

    /** @override */
    validateSafeBrowsingEnhanced() {
      chrome.send('validateSafeBrowsingEnhanced');
    }
  }

  addSingletonGetter(SafeBrowsingBrowserProxyImpl);

