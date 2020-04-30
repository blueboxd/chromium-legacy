// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-a11y-page' is the subpage with the accessibility
 * settings.
 */
Polymer({
  is: 'settings-manage-a11y-page',

  behaviors: [WebUIListenerBehavior, settings.RouteOriginBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    screenMagnifierZoomOptions_: {
      readOnly: true,
      type: Array,
      value() {
        // These values correspond to the i18n values in settings_strings.grdp.
        // If these values get changed then those strings need to be changed as
        // well.
        return [
          {value: 2, name: loadTimeData.getString('screenMagnifierZoom2x')},
          {value: 4, name: loadTimeData.getString('screenMagnifierZoom4x')},
          {value: 6, name: loadTimeData.getString('screenMagnifierZoom6x')},
          {value: 8, name: loadTimeData.getString('screenMagnifierZoom8x')},
          {value: 10, name: loadTimeData.getString('screenMagnifierZoom10x')},
          {value: 12, name: loadTimeData.getString('screenMagnifierZoom12x')},
          {value: 14, name: loadTimeData.getString('screenMagnifierZoom14x')},
          {value: 16, name: loadTimeData.getString('screenMagnifierZoom16x')},
          {value: 18, name: loadTimeData.getString('screenMagnifierZoom18x')},
          {value: 20, name: loadTimeData.getString('screenMagnifierZoom20x')},
        ];
      },
    },

    autoClickDelayOptions_: {
      readOnly: true,
      type: Array,
      value() {
        // These values correspond to the i18n values in settings_strings.grdp.
        // If these values get changed then those strings need to be changed as
        // well.
        return [
          {
            value: 600,
            name: loadTimeData.getString('delayBeforeClickExtremelyShort')
          },
          {
            value: 800,
            name: loadTimeData.getString('delayBeforeClickVeryShort')
          },
          {value: 1000, name: loadTimeData.getString('delayBeforeClickShort')},
          {value: 2000, name: loadTimeData.getString('delayBeforeClickLong')},
          {
            value: 4000,
            name: loadTimeData.getString('delayBeforeClickVeryLong')
          },
        ];
      },
    },

    autoClickMovementThresholdOptions_: {
      readOnly: true,
      type: Array,
      value() {
        return [
          {
            value: 5,
            name: loadTimeData.getString('autoclickMovementThresholdExtraSmall')
          },
          {
            value: 10,
            name: loadTimeData.getString('autoclickMovementThresholdSmall')
          },
          {
            value: 20,
            name: loadTimeData.getString('autoclickMovementThresholdDefault')
          },
          {
            value: 30,
            name: loadTimeData.getString('autoclickMovementThresholdLarge')
          },
          {
            value: 40,
            name: loadTimeData.getString('autoclickMovementThresholdExtraLarge')
          },
        ];
      },
    },

    showExperimentalSwitchAccess_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean(
            'showExperimentalAccessibilitySwitchAccess');
      },
    },

    /**
     * Whether a setting for enabling shelf navigation buttons in tablet mode
     * should be displayed in the accessibility settings.
     * @private
     */
    showShelfNavigationButtonsSettings_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isGuest_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isGuest');
      }
    },

    /**
     * |hasKeyboard_|, |hasMouse_|, and |hasTouchpad_| start undefined so
     * observers don't trigger until they have been populated.
     * @private
     */
    hasKeyboard_: Boolean,

    /** @private */
    hasMouse_: Boolean,

    /** @private */
    hasTouchpad_: Boolean,

    /**
     * Boolean indicating whether shelf navigation buttons should implicitly be
     * enabled in tablet mode - the navigation buttons are implicitly enabled
     * when spoken feedback, automatic clicks, or switch access are enabled.
     * The buttons can also be explicitly enabled by a designated a11y setting.
     * @private
     */
    shelfNavigationButtonsImplicitlyEnabled_: {
      type: Boolean,
      computed: 'computeShelfNavigationButtonsImplicitlyEnabled_(' +
          'prefs.settings.accessibility.value,' +
          'prefs.settings.a11y.autoclick.value,' +
          'prefs.settings.a11y.switch_access.enabled.value)',
    },

    /**
     * The effective pref value that indicates whether shelf navigation buttons
     * are enabled in tablet mode.
     * @type {chrome.settingsPrivate.PrefObject}
     * @private
     */
    shelfNavigationButtonsPref_: {
      type: Object,
      computed: 'getShelfNavigationButtonsEnabledPref_(' +
          'shelfNavigationButtonsImplicitlyEnabled_,' +
          'prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled)',
    },
  },

  observers: [
    'pointersChanged_(hasMouse_, hasTouchpad_)',
  ],

  /** settings.RouteOriginBehavior override */
  route_: settings.routes.MANAGE_ACCESSIBILITY,

  /** @override */
  attached() {
    this.addWebUIListener(
        'has-mouse-changed', this.set.bind(this, 'hasMouse_'));
    this.addWebUIListener(
        'has-touchpad-changed', this.set.bind(this, 'hasTouchpad_'));
    settings.DevicePageBrowserProxyImpl.getInstance().initializePointers();

    this.addWebUIListener(
        'has-hardware-keyboard', this.set.bind(this, 'hasKeyboard_'));
    chrome.send('initializeKeyboardWatcher');
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'initial-data-ready', this.onManageAllyPageReady_.bind(this));
    chrome.send('manageA11yPageReady');

    const r = settings.routes;
    this.addFocusConfig_(r.MANAGE_TTS_SETTINGS, '#ttsSubpageButton');
    this.addFocusConfig_(r.MANAGE_CAPTION_SETTINGS, '#captionsSubpageButton');
    this.addFocusConfig_(
        r.MANAGE_SWITCH_ACCESS_SETTINGS, '#switchAccessSubpageButton');
    this.addFocusConfig_(r.DISPLAY, '#displaySubpageButton');
    this.addFocusConfig_(r.KEYBOARD, '#keyboardSubpageButton');
    this.addFocusConfig_(r.POINTERS, '#pointerSubpageButton');
  },

  /**
   * @param {boolean} hasMouse
   * @param {boolean} hasTouchpad
   * @private
   */
  pointersChanged_(hasMouse, hasTouchpad) {
    this.$.pointerSubpageButton.hidden = !hasMouse && !hasTouchpad;
  },

  /**
   * Updates the Select-to-Speak description text based on:
   *    1. Whether Select-to-Speak is enabled.
   *    2. If it is enabled, whether a physical keyboard is present.
   * @param {boolean} enabled
   * @param {boolean} hasKeyboard
   * @param {string} disabledString String to show when Select-to-Speak is
   *    disabled.
   * @param {string} keyboardString String to show when there is a physical
   *    keyboard
   * @param {string} noKeyboardString String to show when there is no keyboard
   * @private
   */
  getSelectToSpeakDescription_(
      enabled, hasKeyboard, disabledString, keyboardString, noKeyboardString) {
    return !enabled ? disabledString :
                      hasKeyboard ? keyboardString : noKeyboardString;
  },

  /**
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  toggleStartupSoundEnabled_(e) {
    chrome.send('setStartupSoundEnabled', [e.detail]);
  },

  /** @private */
  onManageTtsSettingsTap_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_TTS_SETTINGS);
  },

  /** @private */
  onChromeVoxSettingsTap_() {
    chrome.send('showChromeVoxSettings');
  },

  /** @private */
  onCaptionsClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_CAPTION_SETTINGS);
  },

  /** @private */
  onSelectToSpeakSettingsTap_() {
    chrome.send('showSelectToSpeakSettings');
  },

  /** @private */
  onSwitchAccessSettingsTap_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_SWITCH_ACCESS_SETTINGS);
  },

  /** @private */
  onDisplayTap_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.DISPLAY,
        /* dynamicParams */ null, /* removeSearch */ true);
  },

  /** @private */
  onAppearanceTap_() {
    // Open browser appearance section in a new browser tab.
    window.open('chrome://settings/appearance');
  },

  /** @private */
  onKeyboardTap_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.KEYBOARD,
        /* dynamicParams */ null, /* removeSearch */ true);
  },

  /**
   * @return {boolean} Whether shelf navigation buttons should implicitly be
   *     enabled in tablet mode (due to accessibility settings different than
   *     shelf_navigation_buttons_enabled_in_tablet_mode).
   * @private
   */
  computeShelfNavigationButtonsImplicitlyEnabled_() {
    /**
     * Gets the bool pref value for the provided pref key.
     * @param {string} key
     * @return {boolean}
     */
    const getBoolPrefValue = (key) => {
      const pref = /** @type {chrome.settingsPrivate.PrefObject} */ (
          this.get(key, this.prefs));
      return pref && !!pref.value;
    };

    return getBoolPrefValue('settings.accessibility') ||
        getBoolPrefValue('settings.a11y.autoclick') ||
        getBoolPrefValue('settings.a11y.switch_access.enabled');
  },

  /**
   * Calculates the effective value for "shelf navigation buttons enabled in
   * tablet mode" setting - if the setting is implicitly enabled (by other a11y
   * settings), this will return a stub pref value.
   * @private
   * @return {chrome.settingsPrivate.PrefObject}
   */
  getShelfNavigationButtonsEnabledPref_() {
    if (this.shelfNavigationButtonsImplicitlyEnabled_) {
      return /** @type {!chrome.settingsPrivate.PrefObject}*/ ({
        value: true,
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        key: ''
      });
    }

    return /** @type {chrome.settingsPrivate.PrefObject} */ (this.get(
        'settings.a11y.tablet_mode_shelf_nav_buttons_enabled', this.prefs));
  },

  /** @private */
  onShelfNavigationButtonsLearnMoreClicked_() {
    chrome.metricsPrivate.recordUserAction(
        'Settings_A11y_ShelfNavigationButtonsLearnMoreClicked');
  },

  /**
   * Handles the <code>tablet_mode_shelf_nav_buttons_enabled</code> setting's
   * toggle changes. It updates the backing pref value, unless the setting is
   * implicitly enabled.
   * @private
   */
  updateShelfNavigationButtonsEnabledPref_() {
    if (this.shelfNavigationButtonsImplicitlyEnabled_) {
      return;
    }

    const enabled = this.$.shelfNavigationButtonsEnabledControl.checked;
    this.set(
        'prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value',
        enabled);
    chrome.send('recordSelectedShowShelfNavigationButtonValue', [enabled]);
  },

  /** @private */
  onMouseTap_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.POINTERS,
        /* dynamicParams */ null, /* removeSearch */ true);
  },

  /**
   * Handles updating the visibility of the shelf navigation buttons setting
   * and updating whether startupSoundEnabled is checked.
   * @param {boolean} startup_sound_enabled Whether startup sound is enabled.
   * @param {boolean} tablet_mode_supported Whether tablet mode is supported.
   * @private
   */
  onManageAllyPageReady_(startup_sound_enabled, tablet_mode_supported) {
    this.$.startupSoundEnabled.checked = startup_sound_enabled;
    this.showShelfNavigationButtonsSettings_ = tablet_mode_supported &&
        loadTimeData.getBoolean('showTabletModeShelfNavigationButtonsSettings');
  },
});
