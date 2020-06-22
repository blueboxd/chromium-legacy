// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on ChromeOS specific features.
 */
const CrosView = (function() {
  'use strict';

  /**
   *  Set storing debug logs status.
   *
   *  @private
   */
  function setStoreDebugLogsStatus_(status) {
    $(CrosView.STORE_DEBUG_LOGS_STATUS_ID).innerText = status;
  }


  /**
   *  Set storing combined debug logs status.
   *
   *  @private
   */
  function setStoreCombinedDebugLogsStatus_(status) {
    $(CrosView.STORE_COMBINED_DEBUG_LOGS_STATUS_ID).innerText = status;
  }

  /**
   *  Set storing combined debug logs status.
   *
   *  @private
   */
  function setStoreFeedbackSystemLogsStatus_(status) {
    $(CrosView.STORE_FEEDBACK_SYSTEM_LOGS_STATUS_ID).innerText = status;
  }

  /**
   *  Set status for current debug mode.
   *
   *  @private
   */
  function setNetworkDebugModeStatus_(status) {
    $(CrosView.DEBUG_STATUS_ID).innerText = status;
  }

  /**
   *  Add event listeners for the button for debug logs storing and for buttons
   *  for debug mode selection.
   *
   *  @private
   */
  function addEventListeners_() {
    $(CrosView.STORE_DEBUG_LOGS_ID).addEventListener('click', function(event) {
      $(CrosView.STORE_DEBUG_LOGS_STATUS_ID).innerText = '';
      g_browser.storeDebugLogs();
    }, false);
    $(CrosView.STORE_COMBINED_DEBUG_LOGS_ID)
        .addEventListener('click', function(event) {
          $(CrosView.STORE_COMBINED_DEBUG_LOGS_STATUS_ID).innerText = '';
          g_browser.storeCombinedDebugLogs();
        }, false);
    $(CrosView.STORE_FEEDBACK_SYSTEM_LOGS_ID)
        .addEventListener('click', function(event) {
          $(CrosView.STORE_FEEDBACK_SYSTEM_LOGS_STATUS_ID).innerText = '';
          g_browser.storeFeedbackSystemLogs();
        }, false);

    $(CrosView.DEBUG_WIFI_ID).addEventListener('click', function(event) {
      setNetworkDebugMode_('wifi');
    }, false);
    $(CrosView.DEBUG_ETHERNET_ID).addEventListener('click', function(event) {
      setNetworkDebugMode_('ethernet');
    }, false);
    $(CrosView.DEBUG_CELLULAR_ID).addEventListener('click', function(event) {
      setNetworkDebugMode_('cellular');
    }, false);
    $(CrosView.DEBUG_NONE_ID).addEventListener('click', function(event) {
      setNetworkDebugMode_('none');
    }, false);
  }

  /**
   *  Enables or disables debug mode for a specified subsystem.
   *
   *  @private
   */
  function setNetworkDebugMode_(subsystem) {
    $(CrosView.DEBUG_STATUS_ID).innerText = '';
    g_browser.setNetworkDebugMode(subsystem);
  }

  /**
   *  @constructor
   *  @extends {DivView}
   */
  function CrosView() {
    assertFirstConstructorCall(CrosView);

    // Call superclass's constructor.
    DivView.call(this, CrosView.MAIN_BOX_ID);

    g_browser.addStoreDebugLogsObserver(this);
    g_browser.addSetNetworkDebugModeObserver(this);
    addEventListeners_();
  }

  CrosView.TAB_ID = 'tab-handle-chromeos';
  CrosView.TAB_NAME = 'ChromeOS';
  CrosView.TAB_HASH = '#chromeos';

  CrosView.MAIN_BOX_ID = 'chromeos-view-tab-content';
  CrosView.STORE_DEBUG_LOGS_ID = 'chromeos-view-store-debug-logs';
  CrosView.STORE_DEBUG_LOGS_STATUS_ID = 'chromeos-view-store-debug-logs-status';
  CrosView.STORE_COMBINED_DEBUG_LOGS_ID =
      'chromeos-view-store-combined-debug-logs';
  CrosView.STORE_COMBINED_DEBUG_LOGS_STATUS_ID =
      'chromeos-view-store-combined-debug-logs-status';
  CrosView.STORE_FEEDBACK_SYSTEM_LOGS_ID =
      'chromeos-view-store-feedback-system-logs';
  CrosView.STORE_FEEDBACK_SYSTEM_LOGS_STATUS_ID =
      'chromeos-view-store-feedback-system-logs-status';
  CrosView.DEBUG_WIFI_ID = 'chromeos-view-network-debugging-wifi';
  CrosView.DEBUG_ETHERNET_ID = 'chromeos-view-network-debugging-ethernet';
  CrosView.DEBUG_CELLULAR_ID = 'chromeos-view-network-debugging-cellular';
  CrosView.DEBUG_NONE_ID = 'chromeos-view-network-debugging-none';
  CrosView.DEBUG_STATUS_ID = 'chromeos-view-network-debugging-status';

  cr.addSingletonGetter(CrosView);

  CrosView.prototype = {
    // Inherit from DivView.
    __proto__: DivView.prototype,

    onStoreDebugLogs: setStoreDebugLogsStatus_,
    onStoreCombinedDebugLogs: setStoreCombinedDebugLogsStatus_,
    onStoreFeedbackSystemLogs: setStoreFeedbackSystemLogsStatus_,
    onSetNetworkDebugMode: setNetworkDebugModeStatus_,
  };

  return CrosView;
})();
