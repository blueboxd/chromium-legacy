// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Subpage of settings-multidevice-feature for managing the Smart Lock feature.
 */
cr.exportPath('settings');

cr.define('settings', function() {
  /**
   * The state of the preference controlling Smart Lock's ability to sign-in the
   * user.
   * @enum {string}
   */
  SignInEnabledState = {
    ENABLED: 'enabled',
    DISABLED: 'disabled',
  };

  return {
    SignInEnabledState: SignInEnabledState,
  };
});

Polymer({
  is: 'settings-multidevice-smartlock-subpage',

  behaviors: [
    MultiDeviceFeatureBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @type {?SettingsRoutes} */
    routes: {
      type: Object,
      value: settings.routes,
    },

    /**
     * True if Smart Lock is enabled.
     * @private
     */
    smartLockEnabled_: {
      type: Boolean,
      computed: 'computeIsSmartLockEnabled_(pageContentData)',
    },

    /**
     * Whether Smart Lock may be used to sign-in the user (as opposed to only
     * being able to unlock the user's screen).
     * @private {!settings.SignInEnabledState}
     */
    smartLockSignInEnabled_: {
      type: Object,
      value: settings.SignInEnabledState.DISABLED,
    },

    /**
     * True if the user is allowed to enable Smart Lock sign-in.
     * @private
     */
    smartLockSignInAllowed_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    showPasswordPromptDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * Authentication token provided by password-prompt-dialog.
     * @private {string}
     */
    authToken_: {
      type: String,
      value: '',
    },
  },

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.MultiDeviceBrowserProxyImpl.getInstance();

    this.addWebUIListener(
        'smart-lock-signin-enabled-changed',
        this.updateSmartLockSignInEnabled_.bind(this));

    this.addWebUIListener(
        'smart-lock-signin-allowed-changed',
        this.updateSmartLockSignInAllowed_.bind(this));

    this.browserProxy_.getSmartLockSignInEnabled().then(enabled => {
      this.updateSmartLockSignInEnabled_(enabled);
    });

    this.browserProxy_.getSmartLockSignInAllowed().then(allowed => {
      this.updateSmartLockSignInAllowed_(allowed);
    });
  },

  /**
   * Returns true if Smart Lock is an enabled feature.
   * @return {boolean}
   * @private
   */
  computeIsSmartLockEnabled_: function() {
    return !!this.pageContentData &&
        this.getFeatureState(settings.MultiDeviceFeature.SMART_LOCK) ==
        settings.MultiDeviceFeatureState.ENABLED_BY_USER;
  },

  /**
   * Updates the state of the Smart Lock 'sign-in enabled' toggle.
   * @private
   */
  updateSmartLockSignInEnabled_: function(enabled) {
    this.smartLockSignInEnabled_ = enabled ?
        settings.SignInEnabledState.ENABLED :
        settings.SignInEnabledState.DISABLED;
  },

  /**
   * Updates the Smart Lock 'sign-in enabled' toggle such that disallowing
   * sign-in disables the toggle.
   * @private
   */
  updateSmartLockSignInAllowed_: function(allowed) {
    this.smartLockSignInAllowed_ = allowed;
  },

  /** @private */
  openPasswordPromptDialog_: function() {
    this.showPasswordPromptDialog_ = true;
  },

  /**
   * Sets the Smart Lock 'sign-in enabled' pref based on the value of the
   * radio group representing the pref.
   * @private
   */
  onSmartLockSignInEnabledChanged_: function() {
    const radioGroup = this.$$('cr-radio-group');
    const enabled = radioGroup.selected == settings.SignInEnabledState.ENABLED;

    if (!enabled) {
      // No authentication check is required to disable.
      this.browserProxy_.setSmartLockSignInEnabled(false /* enabled */);
      return;
    }

    // Toggle the enabled state back to disabled, as authentication may not
    // succeed. The toggle state updates automatically by the pref listener.
    radioGroup.selected = settings.SignInEnabledState.DISABLED;
    this.openPasswordPromptDialog_();
  },

  /**
   * Updates the state of the password dialog controller flag when the UI
   * element closes.
   * @private
   */
  onEnableSignInDialogClose_: function() {
    this.showPasswordPromptDialog_ = false;

    // If |this.authToken_| is set when the dialog has been closed, this means
    // that the user entered the correct password into the dialog when
    // attempting to enable SignIn with Smart Lock.
    if (this.authToken_ !== '') {
      this.browserProxy_.setSmartLockSignInEnabled(
          true /* enabled */, this.authToken_);
    }

    // Always require password entry if re-enabling SignIn with Smart Lock.
    this.authToken_ = '';
  },
});
