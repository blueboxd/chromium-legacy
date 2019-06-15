// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * Number of settings sections to show when "More settings" is collapsed.
 * @type {number}
 */
const MAX_SECTIONS_TO_SHOW = 6;

Polymer({
  is: 'print-preview-sidebar',

  behaviors: [
    SettingsBehavior,
    CrContainerShadowBehavior,
    WebUIListenerBehavior,
    print_preview.DarkModeBehavior,
  ],

  properties: {
    cloudPrintErrorMessage: String,

    /** @type {cloudprint.CloudPrintInterface} */
    cloudPrintInterface: Object,

    controlsManaged: Boolean,

    /** @type {print_preview.Destination} */
    destination: {
      type: Object,
      notify: true,
    },

    /** @private {!print_preview.DestinationState} */
    destinationState: {
      type: Number,
      notify: true,
    },

    /** @type {!print_preview.Error} */
    error: {
      type: Number,
      notify: true,
    },

    newPrintPreviewLayout: {
      type: Boolean,
      reflectToAttribute: true,
    },

    pageCount: Number,

    /** @type {!print_preview.State} */
    state: {
      type: Number,
      observer: 'onStateChanged_',
    },

    /** @private {boolean} */
    controlsDisabled_: {
      type: Boolean,
      computed: 'computeControlsDisabled_(state)',
    },

    /** @private {boolean} */
    firstLoad_: {
      type: Boolean,
      value: true,
    },

    /** @private {boolean} */
    isInAppKioskMode_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    settingsExpandedByUser_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    shouldShowMoreSettings_: {
      type: Boolean,
      computed: 'computeShouldShowMoreSettings_(settings.pages.available, ' +
          'settings.copies.available, settings.layout.available, ' +
          'settings.color.available, settings.mediaSize.available, ' +
          'settings.dpi.available, settings.margins.available, ' +
          'settings.pagesPerSheet.available, settings.scaling.available, ' +
          'settings.duplex.available, settings.otherOptions.available, ' +
          'settings.vendorItems.available)',
    },
  },

  /**
   * @param {boolean} appKioskMode
   * @param {string} defaultPrinter The system default printer ID.
   * @param {string} serializedDestinationSelectionRulesStr String with rules
   *     for selecting the default destination.
   * @param {?Array<string>} userAccounts The signed in user accounts.
   * @param {boolean} syncAvailable
   */
  init: function(
      appKioskMode, defaultPrinter, serializedDestinationSelectionRulesStr,
      userAccounts, syncAvailable) {
    this.isInAppKioskMode_ = appKioskMode;
    this.$.destinationSettings.init(
        defaultPrinter, serializedDestinationSelectionRulesStr, userAccounts,
        syncAvailable);
  },

  /**
   * @return {boolean} Whether the controls should be disabled.
   * @private
   */
  computeControlsDisabled_: function() {
    return this.state != print_preview.State.READY;
  },

  /**
   * @return {boolean} Whether to show the "More settings" link.
   * @private
   */
  computeShouldShowMoreSettings_: function() {
    // Destination settings is always available. See if the total number of
    // available sections exceeds the maximum number to show.
    return [
      'pages', 'copies', 'layout', 'color', 'mediaSize', 'margins', 'color',
      'pagesPerSheet', 'scaling', 'dpi', 'duplex', 'otherOptions', 'vendorItems'
    ].reduce((count, setting) => {
      return this.getSetting(setting).available ? count + 1 : count;
    }, 1) > MAX_SECTIONS_TO_SHOW;
  },

  /**
   * @return {boolean} Whether the "more settings" collapse should be expanded.
   * @private
   */
  shouldExpandSettings_: function() {
    if (this.settingsExpandedByUser_ === undefined ||
        this.shouldShowMoreSettings_ === undefined) {
      return false;
    }

    // Expand the settings if the user has requested them expanded or if more
    // settings is not displayed (i.e. less than 6 total settings available).
    return this.settingsExpandedByUser_ || !this.shouldShowMoreSettings_;
  },

  /** @private */
  onPrintButtonFocused_: function() {
    this.firstLoad_ = false;
  },

  onStateChanged_: function() {
    if (this.state !== print_preview.State.PRINTING) {
      return;
    }

    if (this.shouldShowMoreSettings_) {
      print_preview.MetricsContext.printSettingsUi().record(
          this.settingsExpandedByUser_ ?
              print_preview.Metrics.PrintSettingsUiBucket
                  .PRINT_WITH_SETTINGS_EXPANDED :
              print_preview.Metrics.PrintSettingsUiBucket
                  .PRINT_WITH_SETTINGS_COLLAPSED);
    }
  },

  /** @return {boolean} Whether the system dialog link is available. */
  systemDialogLinkAvailable: function() {
    const linkContainer = this.$$('print-preview-link-container');
    return !!linkContainer && linkContainer.systemDialogLinkAvailable();
  },
});
})();
