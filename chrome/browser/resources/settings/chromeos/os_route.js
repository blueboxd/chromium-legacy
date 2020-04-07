// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * Creates Route objects for each path corresponding to CrOS settings content.
   * @return {!OsSettingsRoutes}
   */
  function createOSSettingsRoutes() {
    const r = /** @type {!OsSettingsRoutes} */ ({});

    // Root pages.
    r.BASIC = new settings.Route('/');
    r.ABOUT = new settings.Route('/help');

    r.OS_SIGN_OUT = r.BASIC.createChild('/osSignOut');
    r.OS_SIGN_OUT.isNavigableDialog = true;

    r.OS_SEARCH = r.BASIC.createSection('/osSearch', 'osSearch');
    if (!loadTimeData.getBoolean('isGuest')) {
      r.OS_PEOPLE = r.BASIC.createSection('/osPeople', 'osPeople');
      r.SYNC = r.OS_PEOPLE.createChild('/syncSetup');
      if (!loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
        r.SYNC_ADVANCED = r.SYNC.createChild('/syncSetup/advanced');
      }
    }

    r.INTERNET = r.BASIC.createSection('/internet', 'internet');
    r.INTERNET_NETWORKS = r.INTERNET.createChild('/networks');
    r.NETWORK_DETAIL = r.INTERNET.createChild('/networkDetail');
    r.KNOWN_NETWORKS = r.INTERNET.createChild('/knownNetworks');
    r.BLUETOOTH = r.BASIC.createSection('/bluetooth', 'bluetooth');
    r.BLUETOOTH_DEVICES = r.BLUETOOTH.createChild('/bluetoothDevices');

    r.DEVICE = r.BASIC.createSection('/device', 'device');
    r.POINTERS = r.DEVICE.createChild('/pointer-overlay');
    r.KEYBOARD = r.DEVICE.createChild('/keyboard-overlay');
    r.STYLUS = r.DEVICE.createChild('/stylus');
    r.DISPLAY = r.DEVICE.createChild('/display');
    r.STORAGE = r.DEVICE.createChild('/storage');
    r.EXTERNAL_STORAGE_PREFERENCES =
        r.DEVICE.createChild('/storage/externalStoragePreferences');
    r.POWER = r.DEVICE.createChild('/power');

    // "About" is the only section in About, but we still need to create the
    // route in order to show the subpage on Chrome OS.
    r.ABOUT_ABOUT = r.ABOUT.createSection('/help/about', 'about');
    r.DETAILED_BUILD_INFO = r.ABOUT_ABOUT.createChild('/help/details');

    if (!loadTimeData.getBoolean('isGuest')) {
      r.MULTIDEVICE = r.BASIC.createSection('/multidevice', 'multidevice');
      r.MULTIDEVICE_FEATURES =
          r.MULTIDEVICE.createChild('/multidevice/features');
      r.SMART_LOCK =
          r.MULTIDEVICE_FEATURES.createChild('/multidevice/features/smartLock');

      r.ACCOUNTS = r.OS_PEOPLE.createChild('/accounts');
      r.ACCOUNT_MANAGER = r.OS_PEOPLE.createChild('/accountManager');
      r.KERBEROS_ACCOUNTS = r.OS_PEOPLE.createChild('/kerberosAccounts');
      r.LOCK_SCREEN = r.OS_PEOPLE.createChild('/lockScreen');
      r.FINGERPRINT = r.LOCK_SCREEN.createChild('/lockScreen/fingerprint');
    }

    if (loadTimeData.valueExists('showCrostini') &&
        loadTimeData.getBoolean('showCrostini')) {
      r.CROSTINI = r.BASIC.createSection('/crostini', 'crostini');
      r.CROSTINI_ANDROID_ADB = r.CROSTINI.createChild('/crostini/androidAdb');
      r.CROSTINI_DETAILS = r.CROSTINI.createChild('/crostini/details');
      if (loadTimeData.valueExists('showCrostiniExportImport') &&
          loadTimeData.getBoolean('showCrostiniExportImport')) {
        r.CROSTINI_EXPORT_IMPORT =
            r.CROSTINI_DETAILS.createChild('/crostini/exportImport');
      }
      r.CROSTINI_PORT_FORWARDING =
          r.CROSTINI_DETAILS.createChild('/crostini/portForwarding');
      r.CROSTINI_SHARED_PATHS =
          r.CROSTINI_DETAILS.createChild('/crostini/sharedPaths');
      r.CROSTINI_SHARED_USB_DEVICES =
          r.CROSTINI_DETAILS.createChild('/crostini/sharedUsbDevices');
      if (loadTimeData.valueExists('showCrostiniDiskResize') &&
          loadTimeData.getBoolean('showCrostiniDiskResize')) {
        r.CROSTINI_DISK_RESIZE =
            r.CROSTINI_DETAILS.createChild('/crostini/diskResize');
      }
    }

    if (loadTimeData.valueExists('showPluginVm') &&
        loadTimeData.getBoolean('showPluginVm')) {
      r.PLUGIN_VM = r.BASIC.createSection('/pluginVm', 'pluginVm');
      r.PLUGIN_VM_DETAILS = r.PLUGIN_VM.createChild('/pluginVm/details');
      r.PLUGIN_VM_SHARED_PATHS =
          r.PLUGIN_VM.createChild('/pluginVm/sharedPaths');
    }

    r.GOOGLE_ASSISTANT = r.OS_SEARCH.createChild('/googleAssistant');

    r.ADVANCED = new settings.Route('/advanced');

    r.OS_PRIVACY = r.ADVANCED.createSection('/osPrivacy', 'osPrivacy');

    // Languages and input
    r.OS_LANGUAGES = r.ADVANCED.createSection('/osLanguages', 'osLanguages');
    r.OS_LANGUAGES_DETAILS = r.OS_LANGUAGES.createChild('/osLanguages/details');
    r.OS_LANGUAGES_INPUT_METHODS =
        r.OS_LANGUAGES.createChild('/osLanguages/inputMethods');

    r.OS_PRINTING = r.ADVANCED.createSection('/osPrinting', 'osPrinting');

    r.OS_ACCESSIBILITY = r.ADVANCED.createSection('/osAccessibility', 'a11y');

    if (!loadTimeData.getBoolean('isGuest')) {
      if (loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
        r.OS_SYNC = r.OS_PEOPLE.createChild('/osSync');
      }
      // Personalization
      r.PERSONALIZATION =
          r.BASIC.createSection('/personalization', 'personalization');
      r.CHANGE_PICTURE = r.PERSONALIZATION.createChild('/changePicture');
      r.AMBIENT_MODE = r.PERSONALIZATION.createChild('/ambientMode');

      // Files (analogous to Downloads)
      r.FILES = r.ADVANCED.createSection('/files', 'files');
      r.SMB_SHARES = r.FILES.createChild('/smbShares');
    }

    // Reset
    if (loadTimeData.valueExists('allowPowerwash') &&
        loadTimeData.getBoolean('allowPowerwash')) {
      r.OS_RESET = r.ADVANCED.createSection('/osReset', 'osReset');
    }

    const showAppManagement = loadTimeData.valueExists('showAppManagement') &&
        loadTimeData.getBoolean('showAppManagement');
    const showAndroidApps = loadTimeData.valueExists('androidAppsVisible') &&
        loadTimeData.getBoolean('androidAppsVisible');
    // Apps
    if (showAppManagement || showAndroidApps) {
      r.APPS = r.BASIC.createSection('/apps', 'apps');
      if (showAppManagement) {
        r.APP_MANAGEMENT = r.APPS.createChild('/app-management');
        r.APP_MANAGEMENT_DETAIL =
            r.APP_MANAGEMENT.createChild('/app-management/detail');
      }
      if (showAndroidApps) {
        r.ANDROID_APPS_DETAILS = r.APPS.createChild('/androidAppsDetails');
      }
    }

    r.DATETIME = r.ADVANCED.createSection('/dateTime', 'dateTime');
    r.DATETIME_TIMEZONE_SUBPAGE = r.DATETIME.createChild('/dateTime/timeZone');

    r.CUPS_PRINTERS = r.OS_PRINTING.createChild('/cupsPrinters');

    r.MANAGE_ACCESSIBILITY =
        r.OS_ACCESSIBILITY.createChild('/manageAccessibility');
    if (loadTimeData.getBoolean('showExperimentalAccessibilitySwitchAccess')) {
      r.MANAGE_SWITCH_ACCESS_SETTINGS = r.MANAGE_ACCESSIBILITY.createChild(
          '/manageAccessibility/switchAccess');
    }
    r.MANAGE_TTS_SETTINGS =
        r.MANAGE_ACCESSIBILITY.createChild('/manageAccessibility/tts');

    r.MANAGE_CAPTION_SETTINGS =
        r.MANAGE_ACCESSIBILITY.createChild('/manageAccessibility/captions');
    return r;
  }

  /**
   * @return {!settings.Router} A router with at least those routes common to OS
   *     and browser settings. If the window is not in OS settings (based on
   *     loadTimeData) then browser specific routes are added. If the window is
   *     OS settings or if Chrome OS is using a consolidated settings page for
   *     OS and browser settings then OS specific routes are added.
   */
  function buildRouter() {
    return new settings.Router(createOSSettingsRoutes());
  }

  settings.Router.setInstance(buildRouter());

  window.addEventListener('popstate', function(event) {
    // On pop state, do not push the state onto the window.history again.
    const routerInstance = settings.Router.getInstance();
    routerInstance.setCurrentRoute(
        routerInstance.getRouteForPath(window.location.pathname) ||
            routerInstance.getRoutes().BASIC,
        new URLSearchParams(window.location.search), true);
  });

  // TODO(dpapad): Change to 'get routes() {}' in export when we fix a bug in
  // ChromePass that limits the syntax of what can be returned from cr.define().
  const routes = /** @type {!OsSettingsRoutes} */ (
      settings.Router.getInstance().getRoutes());

  // #cr_define_end
  return {
    buildRouterForTesting: buildRouter,
    routes: routes,
  };
});
