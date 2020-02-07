// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Closure compiler type definitions used by display_manager.js .
 */

/**
 * @typedef {{
 *   enableDebuggingAllowed: (boolean|undefined),
 *   enterDemoModeAllowed: (boolean|undefined),
 *   noAnimatedTransition: (boolean|undefined),
 *   postponeEnrollmentAllowed: (boolean|undefined),
 *   resetAllowed: (boolean|undefined),
 *   startEnrollmentAllowed: (boolean|undefined),
 *   toggleKioskAllowed: (boolean|undefined),
 *   changeRequisitonProhibited: (boolean|undefined),
 * }}
 */
var DisplayManagerScreenAttributes = {};

/**
 * True if showing "enable debugging" is allowed for the screen.
 * @type {boolean|undefined}
 */
DisplayManagerScreenAttributes.enableDebuggingAllowed;

/**
 * True if enabling demo mode is allowed for the screen.
 * @type {boolean|undefined}
 */
DisplayManagerScreenAttributes.enterDemoModeAllowed;

/**
 * True if screen does not use left-current-right animation.
 * @type {boolean|undefined}
 */
DisplayManagerScreenAttributes.noAnimatedTransition;

/**
 * True if enrollment accelerator should schedule postponed enrollment.
 * @type {boolean|undefined}
 */
DisplayManagerScreenAttributes.postponeEnrollmentAllowed;

/**
 * True if device reset is allowed on the screen.
 * @type {boolean|undefined}
 */
DisplayManagerScreenAttributes.resetAllowed;

/**
 * True if enrollment accelerator should start enrollment.
 * @type {boolean|undefined}
 */
DisplayManagerScreenAttributes.startEnrollmentAllowed;

/**
 * True if "enable kiosk" accelerator is allowed.
 * @type {boolean|undefined}
 */
DisplayManagerScreenAttributes.toggleKioskAllowed;

/**
 * True if "enroll hangouts meet" accelerator is prohibited.
 * @type {boolean|undefined}
 */
DisplayManagerScreenAttributes.changeRequisitonProhibited;

/**
 * Possible types of UI.
 * @enum {string}
 */
var DISPLAY_TYPE = {
  UNKNOWN: 'unknown',
  OOBE: 'oobe',
  LOGIN: 'login',
  LOCK: 'lock',
  USER_ADDING: 'user-adding',
  APP_LAUNCH_SPLASH: 'app-launch-splash',
  DESKTOP_USER_MANAGER: 'login-add-user',
  GAIA_SIGNIN: 'gaia-signin'
};

/**
 * Oobe UI state constants.
 * Used to control native UI elements.
 * Should be in sync with login_types.h
 * @enum {number}
 */
var OOBE_UI_STATE = {
  HIDDEN: 0, /* Any OOBE screen without specific state */
  GAIA_SIGNIN: 1,
  ACCOUNT_PICKER: 2,
  WRONG_HWID_WARNING: 3,
  DEPRECATED_SUPERVISED_USER_CREATION_FLOW: 4,
  SAML_PASSWORD_CONFIRM: 5,
  PASSWORD_CHANGED: 6,
  ENROLLMENT: 7,
  ERROR: 8,
  ONBOARDING: 9,
  BLOCKING: 10,
  KIOSK: 11,
  MIGRATION: 12,
};
