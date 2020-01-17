// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-subpage' is the settings subpage for managing Crostini.
 */

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
    isAndroidEnabled_: {
      type: Boolean,
    },

    /**
     * Whether the uninstall options should be displayed.
     * @private {boolean}
     */
    hideCrostiniUninstall_: {
      type: Boolean,
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
  },

  /** settings.RouteOriginBehavior override */
  route_: settings.routes.CROSTINI_DETAILS,

  observers: [
    'onCrostiniEnabledChanged_(prefs.crostini.enabled.value)',
    'onArcEnabledChanged_(prefs.arc.enabled.value)'
  ],

  attached() {
    const callback = (status) => {
      this.hideCrostiniUninstall_ = status;
    };
    this.addWebUIListener('crostini-installer-status-changed', callback);
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniInstallerStatus();
  },

  ready() {
    const r = settings.routes;
    this.addFocusConfig_(r.CROSTINI_SHARED_PATHS, '#crostini-shared-paths');
    this.addFocusConfig_(
        r.CROSTINI_SHARED_USB_DEVICES, '#crostini-shared-usb-devices');
    this.addFocusConfig_(r.CROSTINI_EXPORT_IMPORT, '#crostini-export-import');
    this.addFocusConfig_(r.CROSTINI_ANDROID_ADB, '#crostini-enable-arc-adb');
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

  /**
   * Shows a confirmation dialog when removing crostini.
   * @private
   */
  onRemoveClick_() {
    settings.CrostiniBrowserProxyImpl.getInstance().requestRemoveCrostini();
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
  and_(a, b) {
    return a && b;
  },
});
