// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App install/launch splash screen implementation.
 */

login.createScreen('AppLaunchSplashScreen', 'app-launch-splash', function() {
  return {
    EXTERNAL_API: [
      'toggleNetworkConfig',
      'updateApp',
      'updateMessage',
    ],

    /** Initial UI State for screen */
    getOobeUIInitialState() {
      return OOBE_UI_STATE.KIOSK;
    },

    /** @override */
    decorate() {
      $('splash-config-network').addEventListener('click', function(e) {
        chrome.send('configureNetwork');
      });

      var networkContainer = $('splash-config-network-container');
      networkContainer.addEventListener('transitionend', function(e) {
        if (this.classList.contains('faded'))
          $('splash-config-network').hidden = true;
      }.bind(networkContainer));

      // Ensure the transitionend event gets called after a wait time.
      // The wait time should be inline with the transition duration time
      // defined in css file. The current value in css is 1000ms. To avoid
      // the emulated transitionend firing before real one, a 1050ms
      // delay is used.
      ensureTransitionEndEvent(networkContainer, 1050);
    },

    /** @override */
    onWindowResize() {
      if (Oobe.getInstance().currentScreen !== this)
        return;

      Oobe.getInstance().updateScreenSize(this);
    },

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param {string} data Screen init payload.
     */
    onBeforeShow(data) {
      $('splash-config-network').hidden = true;
      this.toggleNetworkConfig(false);
      this.updateApp(data['appInfo']);

      $('splash-shortcut-info').hidden = !data['shortcutEnabled'];

      Oobe.getInstance().solidBackground = true;
    },

    /**
     * Event handler that is invoked just before the frame is hidden.
     */
    onBeforeHide() {
      Oobe.getInstance().solidBackground = false;
    },

    /**
     * Toggles visibility of the network configuration option.
     * @param {boolean} visible Whether to show the option.
     */
    toggleNetworkConfig(visible) {
      var container = $('splash-config-network-container');
      var currVisible = !container.classList.contains('faded');
      if (currVisible == visible)
        return;

      if (visible) {
        $('splash-config-network').hidden = false;
        container.classList.remove('faded');
      } else {
        container.classList.add('faded');
      }
    },

    /**
     * Updates the app name and icon.
     * @param {Object} app Details of app being launched.
     */
    updateApp(app) {
      $('splash-header').textContent = app.name;
      $('splash-subheader').textContent = app.url;
      $('splash-title').style.backgroundImage = 'url(' + app.iconURL + ')';
    },

    /**
     * Updates the message for the current launch state.
     * @param {string} message Description for current launch state.
     */
    updateMessage(message) {
      $('splash-launch-text').textContent = message;
    }
  };
});
