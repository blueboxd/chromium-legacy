// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on ChromeOS specific features.
 */
const CrosView = (function() {
  'use strict';

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

    g_browser.addSetNetworkDebugModeObserver(this);
    addEventListeners_();
  }

  CrosView.TAB_ID = 'tab-handle-chromeos';
  CrosView.TAB_NAME = 'ChromeOS';
  CrosView.TAB_HASH = '#chromeos';

  CrosView.MAIN_BOX_ID = 'chromeos-view-tab-content';
  CrosView.DEBUG_WIFI_ID = 'chromeos-view-network-debugging-wifi';
  CrosView.DEBUG_ETHERNET_ID = 'chromeos-view-network-debugging-ethernet';
  CrosView.DEBUG_CELLULAR_ID = 'chromeos-view-network-debugging-cellular';
  CrosView.DEBUG_NONE_ID = 'chromeos-view-network-debugging-none';
  CrosView.DEBUG_STATUS_ID = 'chromeos-view-network-debugging-status';

  cr.addSingletonGetter(CrosView);

  CrosView.prototype = {
    // Inherit from DivView.
    __proto__: DivView.prototype,

    onSetNetworkDebugMode: setNetworkDebugModeStatus_,
  };

  return CrosView;
})();
