// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the "SafetyCheck" to interact with
 * the browser.
 */
cr.define('settings', function() {
  /**
   * States of the safety check updates element.
   * Needs to be kept in sync with UpdatesStatus in
   * chrome/browser/ui/webui/settings/safety_check_handler.h
   * @enum {number}
   */
  const SafetyCheckUpdatesStatus = {
    CHECKING: 0,
    UPDATED: 1,
    UPDATING: 2,
    RELAUNCH: 3,
    DISABLED_BY_ADMIN: 4,
    FAILED_OFFLINE: 5,
    FAILED: 6,
  };

  /**
   * States of the safety check passwords element.
   * Needs to be kept in sync with PasswordsStatus in
   * chrome/browser/ui/webui/settings/safety_check_handler.h
   * @enum {number}
   */
  const SafetyCheckPasswordsStatus = {
    CHECKING: 0,
    SAFE: 1,
    COMPROMISED: 2,
    OFFLINE: 3,
    NO_PASSWORDS: 4,
    SIGNED_OUT: 5,
    QUOTA_LIMIT: 6,
    TOO_MANY_PASSWORDS: 7,
    ERROR: 8,
  };

  /**
   * States of the safety check safe browsing element.
   * Needs to be kept in sync with SafeBrowsingStatus in
   * chrome/browser/ui/webui/settings/safety_check_handler.h
   * @enum {number}
   */
  const SafetyCheckSafeBrowsingStatus = {
    CHECKING: 0,
    ENABLED: 1,
    DISABLED: 2,
    DISABLED_BY_ADMIN: 3,
    DISABLED_BY_EXTENSION: 4,
  };

  /**
   * States of the safety check extensions element.
   * Needs to be kept in sync with ExtensionsStatus in
   * chrome/browser/ui/webui/settings/safety_check_handler.h
   * @enum {number}
   */
  const SafetyCheckExtensionsStatus = {
    CHECKING: 0,
    ERROR: 1,
    NO_BLOCKLISTED_EXTENSIONS: 2,
    BLOCKLISTED_ALL_DISABLED: 3,
    BLOCKLISTED_REENABLED_ALL_BY_USER: 4,
    BLOCKLISTED_REENABLED_SOME_BY_USER: 5,
    BLOCKLISTED_REENABLED_ALL_BY_ADMIN: 6,
  };

  /** @interface */
  class SafetyCheckBrowserProxy {
    /** Run the safety check. */
    runSafetyCheck() {}

    /**
     * Get the display string for the safety check parent, showing how long ago
     * safety check last ran.`
     * @param {number} timestamp The timestamp safety check last ran.
     * @return {!Promise<string>}
     */
    getParentRanDisplayString(timestamp) {}
  }

  /** @implements {settings.SafetyCheckBrowserProxy} */
  class SafetyCheckBrowserProxyImpl {
    /** @override */
    runSafetyCheck() {
      chrome.send('performSafetyCheck');
    }

    /** @override */
    getParentRanDisplayString(timestamp) {
      return cr.sendWithPromise('getSafetyCheckRanDisplayString', timestamp);
    }
  }

  cr.addSingletonGetter(SafetyCheckBrowserProxyImpl);

  return {
    SafetyCheckUpdatesStatus,
    SafetyCheckPasswordsStatus,
    SafetyCheckSafeBrowsingStatus,
    SafetyCheckExtensionsStatus,
    SafetyCheckBrowserProxy,
    SafetyCheckBrowserProxyImpl,
  };
});
