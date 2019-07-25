// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cellular_setup', function() {
  /** @enum{string} */
  const PageName = {
    SIM_DETECT: 'sim-detect-page',
    PROVISIONING: 'provisioning-page',
    FINAL: 'final-page',
  };

  return {PageName: PageName};
});

/**
 * Root element for the cellular setup flow. This element interacts with the
 * CellularSetup service to carry out the activation flow. It contains
 * navigation buttons and sub-pages corresponding to each step of the flow.
 */
Polymer({
  is: 'cellular-setup',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Element name of the current selected sub-page.
     * @private {!cellular_setup.PageName}
     */
    selectedPageName_: {
      type: String,
      value: cellular_setup.PageName.SIM_DETECT,
      notify: true,
    },

    /**
     * DOM Element for the current selected sub-page.
     * @private {!SimDetectPageElement|!ProvisioningPageElement|
     *           !FinalPageElement}
     */
    selectedPage_: Object,

    /**
     * Whether error state should be shown for the current page.
     * @private {boolean}
     */
    showError_: {type: Boolean, value: false},

    /**
     * Whether try again should be shown in the button bar.
     * @private {boolean}
     */
    showTryAgainButton_: {type: Boolean, value: false},

    /**
     * Whether finish button should be shown in the button bar.
     * @private {boolean}
     */
    showFinishButton_: {type: Boolean, value: false},

    /**
     * Whether cancel button should be shown in the button bar.
     * @private {boolean}
     */
    showCancelButton_: {type: Boolean, value: false}
  },

  listeners: {
    'backward-nav-requested': 'onBackwardNavRequested_',
    'retry-requested': 'onRetryRequested_',
    'complete-flow-requested': 'onCompleteFlowRequested_',
  },

  /**
   * Provides an interface to the CellularSetup Mojo service.
   * @private {?cellular_setup.MojoInterfaceProvider}
   */
  mojoInterfaceProvider_: null,

  /** @override */
  created: function() {
    this.mojoInterfaceProvider_ =
        cellular_setup.MojoInterfaceProviderImpl.getInstance();
  },

  /** @private */
  onBackwardNavRequested_: function() {
    // TODO(azeemarshad): Add back navigation.
  },

  /** @private */
  onRetryRequested_: function() {
    // TODO(azeemarshad): Add try again logic.
  },

  /** @private */
  onCompleteFlowRequested__: function() {
    // TODO(azeemarshad): Add completion logic.
  },
});
