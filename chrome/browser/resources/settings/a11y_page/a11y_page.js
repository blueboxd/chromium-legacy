// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-a11y-page' is the small section of advanced settings with
 * a link to the web store accessibility page on most platforms, and
 * a subpage with lots of other settings on Chrome OS.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
import '../controls/settings_toggle_button.m.js';
import '../settings_page/settings_animated_pages.m.js';
import '../settings_shared_css.m.js';

// <if expr="not is_macosx and not chromeos">
import './captions_subpage.m.js';
import '../settings_page/settings_subpage.m.js';
// </if>

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.m.js';

// <if expr="is_win or is_macosx">
import {CaptionsBrowserProxyImpl} from './captions_browser_proxy.js';
// </if>


Polymer({
  is: 'settings-a11y-page',

  _template: html`{__html_template__}`,

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
     * Returns true if the 'LiveCaption' media switch is enabled.
     */
    enableLiveCaption_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableLiveCaption');
      },
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
        if (routes.CAPTIONS) {
          map.set(routes.CAPTIONS.path, '#captions');
        }
        return map;
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

  /**
   * @private
   * @param {!Event} event
   */
  onA11yImageLabelsChange_(event) {
    const a11yImageLabelsOn = event.target.checked;
    if (a11yImageLabelsOn) {
      chrome.send('confirmA11yImageLabels');
    }
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.ImageLabels.FromSettings.ToggleSetting',
        a11yImageLabelsOn);
  },

  /**
   * @private
   * @param {!Event} event
   */
  onA11yLiveCaptionChange_(event) {
    const a11yLiveCaptionOn = event.target.checked;
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.LiveCaption.ToggleEnabled', a11yLiveCaptionOn);
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
    CaptionsBrowserProxyImpl.getInstance().openSystemCaptionsDialog();
    // </if>

    // Open the system captions dialog for Windows 10+ or navigate to the
    // caption settings page for older versions of Windows
    // <if expr="is_win">
    if (loadTimeData.getBoolean('isWindows10OrNewer')) {
      CaptionsBrowserProxyImpl.getInstance().openSystemCaptionsDialog();
    } else {
      Router.getInstance().navigateTo(routes.CAPTIONS);
    }
    // </if>

    // Navigate to the caption settings page for Linux as they do not have
    // system caption settings.
    // <if expr="is_linux">
    Router.getInstance().navigateTo(routes.CAPTIONS);
    // </if>
  },
});
