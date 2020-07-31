// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cellularSetup', function() {
  /** @enum{string} */
  const CellularSetupPageName = {
    PSIM_FLOW_UI: 'psim-flow-ui',
  };

  return {
    CellularSetupPageName: CellularSetupPageName,
  };
});

/**
 * @fileoverview Root element for the cellular setup flow. This element wraps
 * the psim setup flow, esim setup flow, and setup flow selection page.
 */
Polymer({
  is: 'cellular-setup',

  properties: {
    /**
     * Element name of the current selected sub-page.
     * @private {!cellularSetup.CellularSetupPageName}
     */
    selectedPageName_: {
      type: String,
      value: cellularSetup.CellularSetupPageName.PSIM_FLOW_UI,
      notify: true,
    }
  },

  listeners: {
    'backward-nav-requested': 'onBackwardNavRequested_',
    'retry-requested': 'onRetryRequested_',
    'complete-flow-requested': 'onCompleteFlowRequested_',
  },

  /** @private */
  onBackwardNavRequested_() {
    // TODO(crbug.com/1093185): Add back navigation.
  },

  /** @private */
  onRetryRequested_() {
    // TODO(crbug.com/1093185): Add try again logic.
  },

  /** @private */
  onCompleteFlowRequested_() {
    // TODO(crbug.com/1093185): Add completion logic.
  },
});
