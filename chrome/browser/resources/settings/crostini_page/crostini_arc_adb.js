// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-arc-adb' is the ARC adb sideloading subpage for Crostini.
 */

Polymer({
  is: 'settings-crostini-arc-adb',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @private {boolean} */
    arcAdbEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    isOwnerProfile_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isOwnerProfile');
      },
    },

    /** @private {boolean} */
    isEnterpriseManaged_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isEnterpriseManaged');
      },
    },

    /** @private {boolean} */
    showConfirmationDialog_: {
      type: Boolean,
      value: false,
    },
  },

  attached: function() {
    this.addWebUIListener(
        'crostini-arc-adb-sideload-status-changed', (enabled) => {
          this.arcAdbEnabled_ = enabled;
        });
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestArcAdbSideloadStatus();
  },

  /**
   * Returns whether the toggle is changeable to the user. Only the device owner
   * is able to change it. Note that the actual guard should be in browser,
   * otherwise a user may bypass this check by inspecting Settings with
   * developer tool.
   * @private
   */
  shouldDisable_: function(isOwnerProfile, isEnterpriseManaged) {
    return !isOwnerProfile || isEnterpriseManaged;
  },

  /** @private */
  getPolicyIndicatorType_: function(isOwnerProfile, isEnterpriseManaged) {
    if (isEnterpriseManaged) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    } else if (!isOwnerProfile) {
      return CrPolicyIndicatorType.OWNER;
    } else {
      return CrPolicyIndicatorType.NONE;
    }
  },

  /** @private */
  getToggleAction_: function(arcAdbEnabled) {
    return arcAdbEnabled ? 'disable' : 'enable';
  },

  /** @private */
  onArcAdbToggleChanged_: function(event) {
    this.showConfirmationDialog_ = true;
  },

  /** @private */
  onConfirmationDialogClose_: function() {
    this.showConfirmationDialog_ = false;
    this.$.arcAdbEnabledButton.checked = this.arcAdbEnabled_;
  },
});
