// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-subpage' is the settings subpage for managing Crostini.
 */

/**
 * The current confirmation state.
 * @enum {string}
 */
const ConfirmationState = {
  NOT_CONFIRMED: 'notConfirmed',
  CONFIRMED: 'confirmed',
};

Polymer({
  is: 'settings-crostini-subpage',

  behaviors:
      [PrefsBehavior, WebUIListenerBehavior, settings.RouteOriginBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Whether export / import UI should be displayed.
     * @private {boolean}
     */
    showCrostiniExportImport_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showCrostiniExportImport');
      },
    },

    /** @private {boolean} */
    showArcAdbSideloading_: {
      type: Boolean,
      computed: 'and_(isArcAdbSideloadingSupported_, isAndroidEnabled_)',
    },

    /** @private {boolean} */
    isArcAdbSideloadingSupported_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('arcAdbSideloadingSupported');
      },
    },

    /** @private {boolean} */
    showCrostiniPortForwarding_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showCrostiniPortForwarding');
      },
    },

    /** @private {boolean} */
    isAndroidEnabled_: {
      type: Boolean,
    },

    /**
     * Whether the uninstall options should be displayed.
     * @private {boolean}
     */
    hideCrostiniUninstall_: {
      type: Boolean,
      computed: 'or_(installerShowing_, upgraderDialogShowing_)',
    },

    /**
     * Whether the button to launch the Crostini container upgrade flow should
     * be shown.
     * @private {boolean}
     */
    showCrostiniContainerUpgrade_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showCrostiniContainerUpgrade');
      },
    },

    /**
     * Whether the button to show the disk resizing view should be shown.
     * @private {boolean}
     */
    showCrostiniDiskResize_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showCrostiniDiskResize');
      },
    },

    /** @private */
    showDiskResizeConfirmationDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the toggle to share the mic with Crostini should be shown.
     * @private {boolean}
     */
    showCrostiniMic_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showCrostiniMic');
      },
    },

    /*
     * Whether the installer is showing.
     * @private {boolean}
     */
    installerShowing_: {
      type: Boolean,
    },

    /**
     * Whether the upgrader dialog is showing.
     * @private {boolean}
     */
    upgraderDialogShowing_: {
      type: Boolean,
    },

    /**
     * Whether the button to launch the Crostini container upgrade flow should
     * be disabled.
     * @private {boolean}
     */
    disableUpgradeButton_: {
      type: Boolean,
      computed: 'or_(installerShowing_, upgraderDialogShowing_)',
    },

    /**
     * Whether the disk resizing dialog is visible or not
     * @private {boolean}
     */
    showDiskResizeDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showCrostiniMicSharingDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    diskSizeLabel_: {
      type: String,
      value: loadTimeData.getString('crostiniDiskSizeCalculating'),
    },

    /** @private {string} */
    diskResizeButtonLabel_: {
      type: String,
      value: loadTimeData.getString('crostiniDiskResizeShowButton'),
    },

    /** @private {string} */
    diskResizeButtonAriaLabel_: {
      type: String,
      value: loadTimeData.getString('crostiniDiskResizeShowButtonAriaLabel'),
    },
  },

  /** settings.RouteOriginBehavior override */
  route_: settings.routes.CROSTINI_DETAILS,


  /** @private {boolean} */
  isDiskUserChosenSize_: false,

  /** @private {!ConfirmationState} */
  diskResizeConfirmationState_: ConfirmationState.NOT_CONFIRMED,

  observers: [
    'onCrostiniEnabledChanged_(prefs.crostini.enabled.value)',
    'onArcEnabledChanged_(prefs.arc.enabled.value)'
  ],

  attached() {
    this.addWebUIListener('crostini-installer-status-changed', (status) => {
      this.installerShowing_ = status;
    });
    this.addWebUIListener('crostini-upgrader-status-changed', (status) => {
      this.upgraderDialogShowing_ = status;
    });
    this.addWebUIListener(
        'crostini-container-upgrade-available-changed', (canUpgrade) => {
          this.showCrostiniContainerUpgrade_ = canUpgrade;
        });
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniInstallerStatus();
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniUpgraderDialogStatus();
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniContainerUpgradeAvailable();
    this.loadDiskInfo_();
  },

  ready() {
    const r = settings.routes;
    this.addFocusConfig_(r.CROSTINI_SHARED_PATHS, '#crostini-shared-paths');
    this.addFocusConfig_(
        r.CROSTINI_SHARED_USB_DEVICES, '#crostini-shared-usb-devices');
    this.addFocusConfig_(r.CROSTINI_EXPORT_IMPORT, '#crostini-export-import');
    this.addFocusConfig_(r.CROSTINI_ANDROID_ADB, '#crostini-enable-arc-adb');
    this.addFocusConfig_(
        r.CROSTINI_PORT_FORWARDING, '#crostini-port-forwarding');
  },

  /** @private */
  onCrostiniEnabledChanged_(enabled) {
    if (!enabled &&
        settings.Router.getInstance().getCurrentRoute() ==
            settings.routes.CROSTINI_DETAILS) {
      settings.Router.getInstance().navigateToPreviousRoute();
    }
  },

  /** @private */
  onArcEnabledChanged_(enabled) {
    this.isAndroidEnabled_ = enabled;
  },

  /** @private */
  onExportImportClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.CROSTINI_EXPORT_IMPORT);
  },

  /** @private */
  onEnableArcAdbClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.CROSTINI_ANDROID_ADB);
  },

  /** @private */
  loadDiskInfo_() {
    // TODO(davidmunro): No magic 'termina' string.
    const vmName = 'termina';
    settings.CrostiniBrowserProxyImpl.getInstance()
        .getCrostiniDiskInfo(vmName, /*requestFullInfo=*/ false)
        .then(
            diskInfo => {
              if (diskInfo.succeeded) {
                this.setResizeLabels_(diskInfo);
              }
            },
            reason => {
              console.log(`Unable to get info: ${reason}`);
            });
  },

  /**
   * @param {!CrostiniDiskInfo} diskInfo
   * @private
   */
  setResizeLabels_(diskInfo) {
    this.isDiskUserChosenSize_ = diskInfo.isUserChosenSize;
    if (this.isDiskUserChosenSize_) {
      if (diskInfo.ticks) {
        this.diskSizeLabel_ = diskInfo.ticks[diskInfo.defaultIndex].label;
      }
      this.diskResizeButtonLabel_ =
          loadTimeData.getString('crostiniDiskResizeShowButton');
      this.diskResizeButtonAriaLabel_ =
          loadTimeData.getString('crostiniDiskResizeShowButtonAriaLabel');
    } else {
      this.diskSizeLabel_ = loadTimeData.getString(
          'crostiniDiskResizeDynamicallyAllocatedSubtext');
      this.diskResizeButtonLabel_ =
          loadTimeData.getString('crostiniDiskReserveSizeButton');
      this.diskResizeButtonAriaLabel_ =
          loadTimeData.getString('crostiniDiskReserveSizeButtonAriaLabel');
    }
  },

  /** @private */
  onDiskResizeClick_() {
    if (!this.isDiskUserChosenSize_ &&
        this.diskResizeConfirmationState_ !== ConfirmationState.CONFIRMED) {
      this.showDiskResizeConfirmationDialog_ = true;
      return;
    }
    this.showDiskResizeDialog_ = true;
  },

  /** @private */
  onDiskResizeDialogClose_() {
    this.showDiskResizeDialog_ = false;
    this.diskResizeConfirmationState_ = ConfirmationState.NOT_CONFIRMED;
    // DiskInfo could have changed.
    this.loadDiskInfo_();
  },

  /** @private */
  onDiskResizeConfirmationDialogClose_() {
    // The on_cancel is followed by on_close, so check cancel didn't happen
    // first.
    if (this.showDiskResizeConfirmationDialog_) {
      this.diskResizeConfirmationState_ = ConfirmationState.CONFIRMED;
      this.showDiskResizeConfirmationDialog_ = false;
      this.showDiskResizeDialog_ = true;
    }
  },

  /** @private */
  onDiskResizeConfirmationDialogCancel_() {
    this.showDiskResizeConfirmationDialog_ = false;
  },

  /**
   * Shows a confirmation dialog when removing crostini.
   * @private
   */
  onRemoveClick_() {
    settings.CrostiniBrowserProxyImpl.getInstance().requestRemoveCrostini();
    settings.recordSettingChange();
  },

  /**
   * Shows the upgrade flow dialog.
   * @private
   */
  onContainerUpgradeClick_() {
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniContainerUpgradeView();
  },

  /** @private */
  onSharedPathsClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.CROSTINI_SHARED_PATHS);
  },

  /** @private */
  onSharedUsbDevicesClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.CROSTINI_SHARED_USB_DEVICES);
  },

  /** @private */
  onPortForwardingClick_: function() {
    settings.Router.getInstance().navigateTo(
        settings.routes.CROSTINI_PORT_FORWARDING);
  },

  /**
   * If a change to the mic settings requires Crostini to be restarted, a
   * dialog is shown.
   * @private
   */
  onMicSharingChange_: function() {
    const proposedValue = /** @type {!SettingsToggleButtonElement} */
        (this.$$('#crostini-mic-sharing')).checked;
    settings.CrostiniBrowserProxyImpl.getInstance()
        .checkCrostiniMicSharingStatus(proposedValue)
        .then(requiresRestart => {
          if (requiresRestart) {
            this.showCrostiniMicSharingDialog_ = true;
          }
        });
  },

  /** @private */
  onCrostiniMicSharingDialogClose_: function() {
    this.showCrostiniMicSharingDialog_ = false;
  },

  /**
   * @private
   * @param {boolean} a
   * @param {boolean} b
   * @return {boolean}
   */
  and_: function(a, b) {
    return a && b;
  },

  /**
   * @private
   * @param {boolean} a
   * @param {boolean} b
   * @return {boolean}
   */
  or_: function(a, b) {
    return a || b;
  },
});
