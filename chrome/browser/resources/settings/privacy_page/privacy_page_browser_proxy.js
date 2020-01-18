// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles interprocess communication for the privacy page. */

/**
 * Contains all possible recorded interactions across privacy settings pages.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with enum of the same name in
 * histograms/enums.xml
 */
settings.SettingsPageInteractions = {
  PRIVACY_SYNC_AND_GOOGLE_SERVICES: 0,
  PRIVACY_CHROME_SIGN_IN: 1,
  PRIVACY_DO_NOT_TRACK: 2,
  PRIVACY_PAYMENT_METHOD: 3,
  PRIVACY_NETWORK_PREDICTION: 4,
  PRIVACY_MANAGE_CERTIFICATES: 5,
  PRIVACY_SECURITY_KEYS: 6,
  PRIVACY_SITE_SETTINGS: 7,
  PRIVACY_CLEAR_BROWSING_DATA: 8,
  // Leave this at the end.
  SETTINGS_MAX_VALUE: 8,
};

/** @typedef {{enabled: boolean, managed: boolean}} */
let MetricsReporting;

cr.define('settings', function() {
  /** @interface */
  class PrivacyPageBrowserProxy {
    // <if expr="_google_chrome and not chromeos">
    /** @return {!Promise<!MetricsReporting>} */
    getMetricsReporting() {}

    /** @param {boolean} enabled */
    setMetricsReportingEnabled(enabled) {}

    // </if>

    // <if expr="is_win or is_macosx">
    /** Invokes the native certificate manager (used by win and mac). */
    showManageSSLCertificates() {}

    // </if>

    /**
     * Helper function that calls recordHistogram for the
     * SettingsPage.SettingsPageInteractions histogram
     */
    recordSettingsPageHistogram(value) {}

    /** @param {boolean} enabled */
    setBlockAutoplayEnabled(enabled) {}
  }

  /**
   * @implements {settings.PrivacyPageBrowserProxy}
   */
  class PrivacyPageBrowserProxyImpl {
    // <if expr="_google_chrome and not chromeos">
    /** @override */
    getMetricsReporting() {
      return cr.sendWithPromise('getMetricsReporting');
    }

    /** @override */
    setMetricsReportingEnabled(enabled) {
      chrome.send('setMetricsReportingEnabled', [enabled]);
    }

    // </if>

    /** @override*/
    recordSettingsPageHistogram(value) {
      chrome.send('metricsHandler:recordInHistogram', [
        'SettingsPage.SettingsPageInteractions', value,
        settings.SettingsPageInteractions.SETTINGS_MAX_VALUE
      ]);
    }

    /** @override */
    setBlockAutoplayEnabled(enabled) {
      chrome.send('setBlockAutoplayEnabled', [enabled]);
    }

    // <if expr="is_win or is_macosx">
    /** @override */
    showManageSSLCertificates() {
      chrome.send('showManageSSLCertificates');
    }
    // </if>
  }

  cr.addSingletonGetter(PrivacyPageBrowserProxyImpl);

  return {
    PrivacyPageBrowserProxy: PrivacyPageBrowserProxy,
    PrivacyPageBrowserProxyImpl: PrivacyPageBrowserProxyImpl,
  };
});
