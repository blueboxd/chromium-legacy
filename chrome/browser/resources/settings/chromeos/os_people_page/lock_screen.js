// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-lock-screen' allows the user to change how they unlock their
 * device.
 *
 * Example:
 *
 * <settings-lock-screen
 *   prefs="{{prefs}}">
 * </settings-lock-screen>
 */

Polymer({
  is: 'settings-lock-screen',

  behaviors: [
    I18nBehavior,
    LockStateBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {type: Object},

    /**
     * setModes_ is a partially applied function that stores the current auth
     * token. It's defined only when the user has entered a valid password.
     * @type {Object|undefined}
     * @private
     */
    setModes_: {
      type: Object,
      observer: 'onSetModesChanged_',
    },

    /**
     * Authentication token provided by lock-screen-password-prompt-dialog.
     * @type {!chrome.quickUnlockPrivate.TokenInfo|undefined}
     */
    authToken: {
      type: Object,
      notify: true,
      observer: 'onAuthTokenChanged_',
    },

    /**
     * writeUma_ is a function that handles writing uma stats. It may be
     * overridden for tests.
     *
     * @type {Function}
     * @private
     */
    writeUma_: {
      type: Object,
      value() {
        return settings.recordLockScreenProgress;
      },
    },

    /**
     * True if quick unlock settings should be displayed on this machine.
     * @private
     */
    quickUnlockEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('quickUnlockEnabled');
      },
      readOnly: true,
    },

    /**
     * True if quick unlock settings are disabled by policy.
     * @private
     */
    quickUnlockDisabledByPolicy_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('quickUnlockDisabledByPolicy');
      },
      readOnly: true,
    },

    /**
     * True if fingerprint unlock settings should be displayed on this machine.
     * @private
     */
    fingerprintUnlockEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('fingerprintUnlockEnabled');
      },
      readOnly: true,
    },

    /** @private */
    numFingerprints_: {
      type: Number,
      value: 0,
    },

    /**
     * Whether notifications on the lock screen are enable by the feature flag.
     * @private
     */
    lockScreenNotificationsEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('lockScreenNotificationsEnabled');
      },
      readOnly: true,
    },

    /**
     * Whether the "hide sensitive notification" option on the lock screen can
     * be enable by the feature flag.
     * @private
     */
    lockScreenHideSensitiveNotificationSupported_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean(
            'lockScreenHideSensitiveNotificationsSupported');
      },
      readOnly: true,
    },

    /** @private */
    showPasswordPromptDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showSetupPinDialog_: Boolean,
  },

  /** @private {?settings.FingerprintBrowserProxy} */
  fingerprintBrowserProxy_: null,

  /** selectedUnlockType is defined in LockStateBehavior. */
  observers: ['selectedUnlockTypeChanged_(selectedUnlockType)'],

  /** @override */
  attached() {
    if (this.shouldAskForPassword_(
            settings.Router.getInstance().getCurrentRoute())) {
      this.openPasswordPromptDialog_();
    }

    this.fingerprintBrowserProxy_ =
        settings.FingerprintBrowserProxyImpl.getInstance();
    this.updateNumFingerprints_();
  },

  /**
   * Overridden from settings.RouteObserverBehavior.
   * @param {!settings.Route} newRoute
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute == settings.routes.LOCK_SCREEN) {
      this.updateUnlockType(/*activeModesChanged=*/ false);
      this.updateNumFingerprints_();
    }

    if (this.shouldAskForPassword_(newRoute)) {
      this.openPasswordPromptDialog_();
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onScreenLockChange_(event) {
    const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
    if (!this.authToken) {
      console.error('Screen lock changed with expired token.');
      target.checked = !target.checked;
      return;
    }
    this.setLockScreenEnabled(this.authToken.token, target.checked);
  },

  /**
   * Called when the unlock type has changed.
   * @param {!string} selected The current unlock type.
   * @private
   */
  selectedUnlockTypeChanged_(selected) {
    if (selected == LockScreenUnlockType.VALUE_PENDING) {
      return;
    }

    if (selected != LockScreenUnlockType.PIN_PASSWORD && this.setModes_) {
      // If the user selects PASSWORD only (which sends an asynchronous
      // setModes_.call() to clear the quick unlock capability), indicate to the
      // user immediately that the quick unlock capability is cleared by setting
      // |hasPin| to false. If there is an error clearing quick unlock, revert
      // |hasPin| to true. This prevents setupPinButton UI delays, except in the
      // small chance that CrOS fails to remove the quick unlock capability. See
      // https://crbug.com/1054327 for details.
      this.hasPin = false;
      this.setModes_.call(null, [], [], function(result) {
        assert(result, 'Failed to clear quick unlock modes');
        // Revert |hasPin| to true in the event setModes fails to set lock state
        // to PASSWORD only.
        this.hasPin = true;
      });
    }
  },

  /** @private */
  onSetModesChanged_() {
    this.maybeReturnToLockPage_();

    if (this.shouldAskForPassword_(
            settings.Router.getInstance().getCurrentRoute())) {
      this.showSetupPinDialog_ = false;
      this.openPasswordPromptDialog_();
    }
  },

  /** @private */
  maybeReturnToLockPage_() {
    const route = settings.Router.getInstance().getCurrentRoute();
    if (route == settings.routes.FINGERPRINT && !this.setModes_) {
      settings.Router.getInstance().navigateTo(settings.routes.LOCK_SCREEN);
    }
  },

  /** @private */
  openPasswordPromptDialog_() {
    this.showPasswordPromptDialog_ = true;
  },

  /** @private */
  onPasswordPromptDialogClose_() {
    this.showPasswordPromptDialog_ = false;
    if (!this.setModes_) {
      settings.Router.getInstance().navigateToPreviousRoute();
    } else if (!this.$$('#unlockType').disabled) {
      cr.ui.focusWithoutInk(assert(this.$$('#unlockType')));
    } else {
      cr.ui.focusWithoutInk(assert(this.$$('#enableLockScreen')));
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onConfigurePin_(e) {
    e.preventDefault();
    this.writeUma_(settings.LockScreenProgress.CHOOSE_PIN_OR_PASSWORD);
    this.showSetupPinDialog_ = true;
  },

  /** @private */
  onSetupPinDialogClose_() {
    this.showSetupPinDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#setupPinButton')));
  },

  /**
   * Returns true if the setup pin section should be shown.
   * @param {!string} selectedUnlockType The current unlock type. Used to let
   *     Polymer know about the dependency.
   * @private
   */
  showConfigurePinButton_(selectedUnlockType) {
    return selectedUnlockType === LockScreenUnlockType.PIN_PASSWORD;
  },

  /**
   * @param {boolean} hasPin
   * @private
   */
  getSetupPinText_(hasPin) {
    if (hasPin) {
      return this.i18n('lockScreenChangePinButton');
    }
    return this.i18n('lockScreenSetupPinButton');
  },

  /** @private */
  getDescriptionText_() {
    if (this.numFingerprints_ > 0) {
      return this.i18n(
          'lockScreenNumberFingerprints', this.numFingerprints_.toString());
    }

    return this.i18n('lockScreenEditFingerprintsDescription');
  },

  /** @private */
  onEditFingerprints_() {
    settings.Router.getInstance().navigateTo(settings.routes.FINGERPRINT);
  },

  /**
   * @param {!settings.Route} route
   * @return {boolean} Whether the password dialog should be shown.
   * @private
   */
  shouldAskForPassword_(route) {
    return route == settings.routes.LOCK_SCREEN && !this.setModes_;
  },

  /** @private */
  updateNumFingerprints_() {
    if (this.fingerprintUnlockEnabled_ && this.fingerprintBrowserProxy_) {
      this.fingerprintBrowserProxy_.getNumFingerprints().then(
          numFingerprints => {
            this.numFingerprints_ = numFingerprints;
          });
    }
  },

  /**
   * Looks up the translation id, which depends on PIN login support.
   * @param {boolean} hasPinLogin
   * @private
   */
  selectLockScreenOptionsString(hasPinLogin) {
    if (hasPinLogin) {
      return this.i18n('lockScreenOptionsLoginLock');
    }
    return this.i18n('lockScreenOptionsLock');
  },

  /**
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e
   * @private
   * */
  onAuthTokenObtained_(e) {
    this.authToken = e.detail;
  },

  /** @private */
  onAuthTokenChanged_() {
    if (this.authToken === undefined) {
      this.setModes_ = undefined;
      return;
    }
    this.setModes_ = (modes, credentials, onComplete) => {
      this.quickUnlockPrivate.setModes(
          this.authToken.token, modes, credentials, () => {
            let result = true;
            if (chrome.runtime.lastError) {
              console.error(
                  'setModes failed: ' + chrome.runtime.lastError.message);
              result = false;
            }
            onComplete(result);
          });
    };
  },
});
