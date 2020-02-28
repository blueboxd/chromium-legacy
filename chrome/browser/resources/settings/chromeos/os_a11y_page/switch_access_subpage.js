// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'switch-access-subpage' is the collapsible section containing
 * Switch Access settings.
 */

(function() {

/**
 * Available switch assignment values.
 * @enum {number}
 * @const
 */
const SwitchAccessAssignmentValue = {
  NONE: 0,
  SPACE: 1,
  ENTER: 2,
};

/**
 * Available commands.
 * @const
 */
const SWITCH_ACCESS_COMMANDS = ['next', 'previous', 'select'];

/**
 * The portion of the setting name common to all Switch Access preferences.
 * @const
 */
const PREFIX = 'settings.a11y.switch_access.';

/**
 * The ending of the setting name for all key code preferences.
 * @const
 */
const KEY_CODE_SUFFIX = '.key_codes';

/**
 * The ending of the setting name for all preferences referring to
 * Switch Access command settings.
 * @const
 */
const COMMAND_SUFFIX = '.setting';

/** @type {!Array<number>} */
const AUTO_SCAN_SPEED_RANGE_MS = [
  500,  600,  700,  800,  900,  1000, 1100, 1200, 1300, 1400, 1500, 1600,
  1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800,
  2900, 3000, 3100, 3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000
];

/**
 * This function extracts the segment of a preference key after the fixed prefix
 * and returns it. In cases where the preference is Switch Access command
 * setting preference, it corresponds to the command name.
 *
 * @param {!chrome.settingsPrivate.PrefObject} pref
 * @return {string}
 */
function getCommandNameFromCommandPref(pref) {
  const nameStartIndex = PREFIX.length;
  const nameEndIndex = pref.key.indexOf('.', nameStartIndex);
  return pref.key.substring(nameStartIndex, nameEndIndex);
}

/**
 * @param {!Array<number>} ticksInMs
 * @return {!Array<!cr_slider.SliderTick>}
 */
function ticksWithLabelsInSec(ticksInMs) {
  // Dividing by 1000 to convert milliseconds to seconds for the label.
  return ticksInMs.map(x => ({label: `${x / 1000}`, value: x}));
}

Polymer({
  is: 'settings-switch-access-subpage',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {Array<number>} */
    autoScanSpeedRangeMs_: {
      readOnly: true,
      type: Array,
      value: ticksWithLabelsInSec(AUTO_SCAN_SPEED_RANGE_MS),
    },

    /** @private {Object} */
    formatter_: {
      type: Object,
      value() {
        // navigator.language actually returns a locale, not just a language.
        const locale = window.navigator.language;
        const options = {minimumFractionDigits: 1, maximumFractionDigits: 1};
        return new Intl.NumberFormat(locale, options);
      },
    },

    /** @private {number} */
    maxScanSpeedMs_: {readOnly: true, type: Number, value: 4000},

    /** @private {string} */
    maxScanSpeedLabelSec_: {
      readOnly: true,
      type: String,
      value() {
        return this.scanSpeedStringInSec_(this.maxScanSpeedMs_);
      },
    },

    /** @private {number} */
    minScanSpeedMs_: {readOnly: true, type: Number, value: 500},

    /** @private {string} */
    minScanSpeedLabelSec_: {
      readOnly: true,
      type: String,
      value() {
        return this.scanSpeedStringInSec_(this.minScanSpeedMs_);
      },
    },

    /** @private {Array<Object>} */
    switchAssignOptions_: {
      readOnly: true,
      type: Array,
      value() {
        return [
          {
            value: SwitchAccessAssignmentValue.NONE,
            name: this.i18n('switchAssignOptionNone')
          },
          {
            value: SwitchAccessAssignmentValue.SPACE,
            name: this.i18n('switchAssignOptionSpace')
          },
          {
            value: SwitchAccessAssignmentValue.ENTER,
            name: this.i18n('switchAssignOptionEnter')
          },
        ];
      },
    },
  },

  /** @override */
  created() {
    chrome.settingsPrivate.onPrefsChanged.addListener((prefs) => {
      for (const pref of prefs) {
        if (!pref.key.includes(PREFIX) || !pref.key.includes(COMMAND_SUFFIX)) {
          continue;
        }
        const commandName = getCommandNameFromCommandPref(pref);
        if (SWITCH_ACCESS_COMMANDS.includes(commandName)) {
          this.onSwitchAssigned_(pref);
        }
      }
    });
  },

  /**
   * @return {string}
   * @private
   */
  currentSpeed_() {
    const speed = this.getPref(PREFIX + 'auto_scan.speed_ms').value;
    if (typeof speed != 'number') {
      return '';
    }
    return this.scanSpeedStringInSec_(speed);
  },

  /**
   * @return {boolean} Whether to show settings for auto-scan within the
   *     keyboard.
   * @private
   */
  showKeyboardScanSettings_() {
    const improvedTextInputEnabled = loadTimeData.getBoolean(
        'showExperimentalAccessibilitySwitchAccessImprovedTextInput');
    const autoScanEnabled = /** @type {boolean} */
        (this.getPref(PREFIX + 'auto_scan.enabled').value);
    return improvedTextInputEnabled && autoScanEnabled;
  },

  /** @param {!chrome.settingsPrivate.PrefObject} newPref */
  onSwitchAssigned_(newPref) {
    const command = getCommandNameFromCommandPref(newPref);

    if (newPref.value !== SwitchAccessAssignmentValue.NONE) {
      // When setting to a value, enforce that no other command can have that
      // value.
      for (const val of SWITCH_ACCESS_COMMANDS) {
        if (val === command) {
          continue;
        }
        if (this.getPref(PREFIX + val + COMMAND_SUFFIX).value ===
            newPref.value) {
          chrome.settingsPrivate.setPref(
              PREFIX + val + COMMAND_SUFFIX, SwitchAccessAssignmentValue.NONE);
        }
      }
    }

    // Because of complexities with mapping a ListPref to a settings-dropdown,
    // we instead store two distinct preferences (one for the dropdown selection
    // and one with the key codes that Switch Access intercepts). The following
    // code sets the key code preference based on the dropdown preference.
    switch (newPref.value) {
      case SwitchAccessAssignmentValue.NONE:
        chrome.settingsPrivate.setPref(PREFIX + command + KEY_CODE_SUFFIX, []);
        break;
      case SwitchAccessAssignmentValue.SPACE:
        chrome.settingsPrivate.setPref(
            PREFIX + command + KEY_CODE_SUFFIX, [32]);
        break;
      case SwitchAccessAssignmentValue.ENTER:
        chrome.settingsPrivate.setPref(
            PREFIX + command + KEY_CODE_SUFFIX, [13]);
        break;
    }
  },

  /**
   * @param {number} scanSpeedValueMs
   * @return {string} a string representing the scan speed in seconds.
   * @private
   */
  scanSpeedStringInSec_(scanSpeedValueMs) {
    const scanSpeedValueSec = scanSpeedValueMs / 1000;
    return this.i18n(
        'durationInSeconds', this.formatter_.format(scanSpeedValueSec));
  },
});
})();
