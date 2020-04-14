// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying network selection OOBE dialog.
 */

Polymer({
  is: 'oobe-network-md',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  observers:
      ['onDemoModeSetupChanged_(isDemoModeSetup, offlineDemoModeEnabled)'],

  properties: {
    /**
     * Whether network dialog is shown as a part of demo mode setup flow.
     * Additional custom elements can be displayed on network list in demo mode
     * setup.
     * @type {boolean}
     */
    isDemoModeSetup: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether offline demo mode is enabled. If it is enabled offline setup
     * option will be shown in UI.
     * @type {boolean}
     */
    offlineDemoModeEnabled: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether device is connected to the network.
     * @type {boolean}
     * @private
     */
    isConnected_: {
      type: Boolean,
      value: false,
    },
  },

  /** Called when dialog is shown. */
  onBeforeShow() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
    this.$.networkSelectLogin.onBeforeShow();
  },

  /** Called when dialog is hidden. */
  onBeforeHide() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeHide)
        behavior.onBeforeHide.call(this);
    });
    this.$.networkSelectLogin.onBeforeHide();
  },

  /** @override */
  ready() {
    this.updateLocalizedContent();
  },

  /** Shows the dialog. */
  show() {
    this.$.networkDialog.show();
  },

  focus() {
    this.$.networkDialog.focus();
  },

  /** Updates localized elements of the UI. */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /**
   * Returns element of the network list selected by the query.
   * Used to simplify testing.
   * @param {string} query
   * @return {NetworkList.NetworkListItemType}
   */
  getNetworkListItemWithQueryForTest(query) {
    let networkList =
        this.$.networkSelectLogin.$$('#networkSelect').getNetworkListForTest();
    assert(networkList);
    return networkList.querySelector(query);
  },

  /**
   * Returns element of the network list with the given name.
   * Used to simplify testing.
   * @param {string} name
   * @return {?NetworkList.NetworkListItemType}
   */
  getNetworkListItemByNameForTest(name) {
    return this.$.networkSelectLogin.$$('#networkSelect')
        .getNetworkListItemByNameForTest(name);
  },

  /**
   * Called after dialog is shown. Refreshes the list of the networks.
   * @private
   */
  onShown_() {
    this.$.networkSelectLogin.refresh();
    this.async(function() {
      if (this.isConnected_)
        this.$.nextButton.focus();
      else
        this.$.networkSelectLogin.focus();
    }.bind(this), 300);
    // Timeout is a workaround to correctly propagate focus to
    // RendererFrameHostImpl see https://crbug.com/955129 for details.
  },

  /**
   * Next button click handler.
   * @private
   */
  onNextClicked_() {
    chrome.send('login.NetworkScreen.userActed', ['continue']);
  },

  /**
   * Back button click handler.
   * @private
   */
  onBackClicked_() {
    chrome.send('login.NetworkScreen.userActed', ['back']);
  },

  /**
   * Updates custom elements on network list when demo mode setup properties
   * changed.
   * @private
   */
  onDemoModeSetupChanged_() {
    this.$.networkSelectLogin.isOfflineDemoModeSetup =
        this.isDemoModeSetup && this.offlineDemoModeEnabled;
  },

  /**
   * This is called when network setup is done.
   * @private
   */
  onNetworkConnected_() {
    chrome.send('login.NetworkScreen.userActed', ['continue']);
  },
});
