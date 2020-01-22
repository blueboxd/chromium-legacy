// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles metrics for the settings pages. */

cr.define('settings', function() {
  /**
   * Contains all possible recorded interactions across privacy settings pages.
   *
   * These values are persisted to logs. Entries should not be renumbered and
   * numeric values should never be reused.
   *
   * Must be kept in sync with enum of the same name in
   * histograms/enums.xml
   * @enum {number}
   */
  const SettingsPageInteractions = {
    PRIVACY_SYNC_AND_GOOGLE_SERVICES: 0,
    PRIVACY_CHROME_SIGN_IN: 1,
    PRIVACY_DO_NOT_TRACK: 2,
    PRIVACY_PAYMENT_METHOD: 3,
    PRIVACY_NETWORK_PREDICTION: 4,
    PRIVACY_MANAGE_CERTIFICATES: 5,
    PRIVACY_SECURITY_KEYS: 6,
    PRIVACY_SITE_SETTINGS: 7,
    PRIVACY_CLEAR_BROWSING_DATA: 8,
    PRIVACY_SAFE_BROWSING: 9,
    PRIVACY_PASSWORD_CHECK: 10,
    PRIVACY_IMPROVE_SECURITY: 11,
    PRIVACY_SITE_SETTINGS_COOKIES: 12,
    PRIVACY_SITE_SETTINGS_LOCATION: 13,
    PRIVACY_SITE_SETTINGS_CAMERA: 14,
    PRIVACY_SITE_SETTINGS_MICROPHONE: 15,
    PRIVACY_SITE_SETTINGS_SENSORS: 16,
    PRIVACY_SITE_SETTINGS_NOTIFICATIONS: 17,
    PRIVACY_SITE_SETTINGS_JAVASCRIPT: 18,
    PRIVACY_SITE_SETTINGS_FLASH: 19,
    PRIVACY_SITE_SETTINGS_IMAGES: 20,
    PRIVACY_SITE_SETTINGS_POPUPS: 21,
    PRIVACY_SITE_SETTINGS_ADS: 22,
    PRIVACY_SITE_SETTINGS_BACKGROUND_SYNC: 23,
    PRIVACY_SITE_SETTINGS_SOUND: 24,
    PRIVACY_SITE_SETTINGS_AUTOMATIC_DOWNLOADS: 25,
    PRIVACY_SITE_SETTINGS_UNSANDBOXED_PLUGINS: 26,
    PRIVACY_SITE_SETTINGS_HANDLERS: 27,
    PRIVACY_SITE_SETTINGS_MIDI_DEVICES: 28,
    PRIVACY_SITE_SETTINGS_ZOOM_LEVELS: 29,
    PRIVACY_SITE_SETTINGS_USB_DEVICES: 30,
    PRIVACY_SITE_SETTINGS_SERIAL_PORTS: 31,
    PRIVACY_SITE_SETTINGS_NATIVE_FILE_SYSTEM_WRITE: 32,
    PRIVACY_SITE_SETTINGS_PDF_DOCUMENTS: 33,
    PRIVACY_SITE_SETTINGS_PROTECTED_CONTENT: 34,
    PRIVACY_SITE_SETTINGS_CLIPBOARD: 35,
    PRIVACY_SITE_SETTINGS_PAYMENT_HANDLER: 36,
    PRIVACY_SITE_SETTINGS_MIXEDSCRIPT: 37,
    PRIVACY_SITE_SETTINGS_BLUETOOTH_SCANNING: 38,
    // Leave this at the end.
    SETTINGS_MAX_VALUE: 38,
  };

  /** @interface */
  class MetricsBrowserProxy {
    /**
     * Helper function that calls recordHistogram for the
     * SettingsPage.SettingsPageInteractions histogram
     * @param {!settings.SettingsPageInteractions} interaction
     */
    recordSettingsPageHistogram(interaction) {}
  }

  /**
   * @implements {settings.MetricsBrowserProxy}
   */
  class MetricsBrowserProxyImpl {
    /** @override*/
    recordSettingsPageHistogram(interaction) {
      chrome.send('metricsHandler:recordInHistogram', [
        'SettingsPage.SettingsPageInteractions', interaction,
        settings.SettingsPageInteractions.SETTINGS_MAX_VALUE
      ]);
    }
  }

  cr.addSingletonGetter(MetricsBrowserProxyImpl);

  return {
    MetricsBrowserProxy,
    MetricsBrowserProxyImpl,
    SettingsPageInteractions
  };
});