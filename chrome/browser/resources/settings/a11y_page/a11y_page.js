// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-a11y-page' is the small section of advanced settings with
 * a link to the web store accessibility page on most platforms, and
 * a subpage with lots of other settings on Chrome OS.
 */
Polymer({
  is: 'settings-a11y-page',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Whether to show accessibility labels settings.
     */
    showAccessibilityLabelsSetting_: {
      type: Boolean,
      value: false,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (settings.routes.CAPTIONS) {
          map.set(settings.routes.CAPTIONS.path, '#captions');
        }
        return map;
      },
    },

    /**
     * Whether to show the link to caption settings.
     * @private {boolean}
     */
    showCaptionSettings_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableCaptionSettings');
      },
    },

    /**
     * Whether the caption settings link opens externally.
     * @private {boolean}
     */
    captionSettingsOpensExternally_: {
      type: Boolean,
      value() {
        let opensExternally = false;
        // <if expr="is_macosx">
        opensExternally = true;
        // </if>

        // <if expr="is_win">
        opensExternally = loadTimeData.getBoolean('isWindows10OrNewer');
        // </if>

        return opensExternally;
      },
    },
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'screen-reader-state-changed',
        this.onScreenReaderStateChanged_.bind(this));
    chrome.send('getScreenReaderState');
  },

  /**
   * @private
   * @param {boolean} hasScreenReader Whether a screen reader is enabled.
   */
  onScreenReaderStateChanged_(hasScreenReader) {
    // TODO(katie): Remove showExperimentalA11yLabels flag before launch.
    this.showAccessibilityLabelsSetting_ = hasScreenReader &&
        loadTimeData.getBoolean('showExperimentalA11yLabels');
  },

  /** @private */
  onToggleAccessibilityImageLabels_() {
    const a11yImageLabelsOn = this.$.a11yImageLabels.checked;
    if (a11yImageLabelsOn) {
      chrome.send('confirmA11yImageLabels');
    }
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.ImageLabels.FromSettings.ToggleSetting',
        a11yImageLabelsOn);
  },

  // <if expr="chromeos">
  /** @private */
  onManageSystemAccessibilityFeaturesTap_() {
    window.location.href = 'chrome://os-settings/manageAccessibility';
  },
  // </if>

  /** private */
  onMoreFeaturesLinkClick_() {
    window.open(
        'https://chrome.google.com/webstore/category/collection/accessibility');
  },

  /** @private */
  onCaptionsClick_() {
    // Open the system captions dialog for Mac.
    // <if expr="is_macosx">
    settings.CaptionsBrowserProxyImpl.getInstance().openSystemCaptionsDialog();
    // </if>

    // Open the system captions dialog for Windows 10+ or navigate to the
    // caption settings page for older versions of Windows
    // <if expr="is_win">
    if (loadTimeData.getBoolean('isWindows10OrNewer')) {
      settings.CaptionsBrowserProxyImpl.getInstance()
          .openSystemCaptionsDialog();
    } else {
      settings.Router.getInstance().navigateTo(settings.routes.CAPTIONS);
    }
    // </if>

    // Navigate to the caption settings page for Linux as they do not have
    // system caption settings.
    // <if expr="is_linux">
    settings.Router.getInstance().navigateTo(settings.routes.CAPTIONS);
    // </if>
  },
});
