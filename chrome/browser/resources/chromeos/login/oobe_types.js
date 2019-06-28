// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * This file contains typedefs for chromeOS OOBE properties.
 */

var OobeTypes = {};

/**
 * ChromeOS OOBE language descriptor.
 * @typedef {{
 *   code: (string|undefined),
 *   displayName: (string|undefined),
 *   nativeDisplayName: (string|undefined),
 *   optionGroupName: (string|undefined),
 *   selected: boolean,
 *   textDirection: (string|undefined),
 *   title: string,
 *   value: string,
 * }}
 */
OobeTypes.LanguageDsc;

/**
 * ChromeOS OOBE input method descriptor.
 * @typedef {{
 *   optionGroupName: (string|undefined),
 *   selected: boolean,
 *   title: string,
 *   value: string,
 * }}
 */
OobeTypes.IMEDsc;

/**
 * ChromeOS OOBE demo country descriptor.
 * @typedef {{
 *   value: !string,
 *   title: (string|undefined),
 *   selected: (boolean|undefined),
 * }}
 */
OobeTypes.DemoCountryDsc;

/**
 * A set of flags of accessibility options for ChromeOS OOBE.
 * @typedef {{
 *   highContrastEnabled: boolean,
 *   spokenFeedbackEnabled: boolean,
 *   screenMagnifierEnabled: boolean,
 *   largeCursorEnabled: boolean,
 *   virtualKeyboardEnabled: boolean,
 * }}
 */
OobeTypes.A11yStatuses;

/**
 * OOBE screen object (which is created in ui/login/screen.js )
 * @typedef {!Object}
 */
OobeTypes.Screen;

/**
 * Timezone ID.
 * @typedef {!String}
 */
OobeTypes.Timezone;

/**
 * ChromeOS timezone descriptor.
 * @typedef {{
 *   value: (OobeTypes.Timezone|undefined),
 *   title: (String|undefined),
 *   selected: (boolean|undefined),
 * }}
 */
OobeTypes.TimezoneDsc;

/**
 * OOBE configuration, allows automation during OOBE.
 * Keys are also listed in chrome/browser/chromeos/login/configuration_keys.h
 * @typedef {{
 *   language: (string|undefined),
 *   inputMethod: (string|undefined),
 *   welcomeNext: (boolean|undefined),
 *   enableDemoMode: (boolean|undefined),
 *   demoPreferencesNext: (boolean|undefined),
 *   networkSelectGuid: (string|undefined),
 *   networkOfflineDemo: (boolean|undefined),
 *   eulaAutoAccept: (boolean|undefined),
 *   eulaSendStatistics: (boolean|undefined),
 *   networkUseConnected: (boolean|undefined),
 *   arcTosAutoAccept: (boolean|undefined),
 * }}
 */
OobeTypes.OobeConfiguration;

/**
 * Specifies the type of the information that is requested by the security token
 * PIN dialog.
 * @enum {number}
 */
OobeTypes.SecurityTokenPinDialogType = {
  PIN: 0,
  PUK: 1,
};

/**
 * Specifies the type of the error that is displayed in the security token PIN
 * dialog.
 * @enum {number}
 */
OobeTypes.SecurityTokenPinDialogErrorType = {
  UNKNOWN_ERROR: 0,
  INVALID_PIN: 1,
  INVALID_PUK: 2,
  MAX_ATTEMPTS_EXCEEDED: 3,
};

/**
 * Configuration of the security token PIN dialog.
 * @typedef {{
 *   type: OobeTypes.SecurityTokenPinDialogType,
 *   errorType: (OobeTypes.SecurityTokenPinDialogErrorType|undefined),
 *   attemptsLeft: number,
 * }}
 */
OobeTypes.SecurityTokenPinDialogParameters;
