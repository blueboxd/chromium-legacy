// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * Creates Route objects for each path corresponding to CrOS settings content.
   * @return {!SettingsRoutes}
   */
  function createOSSettingsRoutes() {
    /** @type {!SettingsRoutes} */
    const r = {};

    // Root pages.
    r.BASIC = new settings.Route('/');
    r.ABOUT = new settings.Route('/help');

    r.SIGN_OUT = r.BASIC.createChild('/signOut');
    r.SIGN_OUT.isNavigableDialog = true;

    r.SEARCH = r.BASIC.createSection('/search', 'search');
    if (!loadTimeData.getBoolean('isGuest')) {
      r.PEOPLE = r.BASIC.createSection('/people', 'people');
      r.SYNC = r.PEOPLE.createChild('/syncSetup');
      r.SYNC_ADVANCED = r.SYNC.createChild('/syncSetup/advanced');
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

      r.ACCOUNTS = r.PEOPLE.createChild('/accounts');
      r.ACCOUNT_MANAGER = r.PEOPLE.createChild('/accountManager');
      r.KERBEROS_ACCOUNTS = r.PEOPLE.createChild('/kerberosAccounts');
      r.LOCK_SCREEN = r.PEOPLE.createChild('/lockScreen');
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
      r.CROSTINI_SHARED_PATHS =
          r.CROSTINI_DETAILS.createChild('/crostini/sharedPaths');
      r.CROSTINI_SHARED_USB_DEVICES =
          r.CROSTINI_DETAILS.createChild('/crostini/sharedUsbDevices');
    }

    if (loadTimeData.valueExists('showPluginVm') &&
        loadTimeData.getBoolean('showPluginVm')) {
      r.PLUGIN_VM = r.BASIC.createSection('/pluginVm', 'pluginVm');
      r.PLUGIN_VM_DETAILS = r.PLUGIN_VM.createChild('/pluginVm/details');
      r.PLUGIN_VM_SHARED_PATHS =
          r.PLUGIN_VM.createChild('/pluginVm/sharedPaths');
    }

    r.GOOGLE_ASSISTANT = r.SEARCH.createChild('/googleAssistant');

    r.ADVANCED = new settings.Route('/advanced');

    r.PRIVACY = r.ADVANCED.createSection('/privacy', 'privacy');

    // Languages and input
    r.LANGUAGES = r.ADVANCED.createSection('/languages', 'languages');
    r.LANGUAGES_DETAILS = r.LANGUAGES.createChild('/languages/details');
    r.INPUT_METHODS =
        r.LANGUAGES_DETAILS.createChild('/languages/inputMethods');

    r.PRINTING = r.ADVANCED.createSection('/printing', 'printing');

    r.ACCESSIBILITY = r.ADVANCED.createSection('/accessibility', 'a11y');

    if (!loadTimeData.getBoolean('isGuest')) {
      if (loadTimeData.valueExists('splitSettingsSyncEnabled') &&
          loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
        r.OS_SYNC = r.PEOPLE.createChild('/osSync');
      }
      // Personalization
      r.PERSONALIZATION =
          r.BASIC.createSection('/personalization', 'personalization');
      r.CHANGE_PICTURE = r.PERSONALIZATION.createChild('/changePicture');

      // Files (analogous to Downloads)
      r.FILES = r.ADVANCED.createSection('/files', 'files');
      r.SMB_SHARES = r.FILES.createChild('/smbShares');
    }

    // Reset
    if (loadTimeData.valueExists('allowPowerwash') &&
        loadTimeData.getBoolean('allowPowerwash')) {
      r.RESET = r.ADVANCED.createSection('/reset', 'reset');
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

    r.CUPS_PRINTERS = r.PRINTING.createChild('/cupsPrinters');

    r.MANAGE_ACCESSIBILITY =
        r.ACCESSIBILITY.createChild('/manageAccessibility');
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
        /** @type {!settings.Route} */ (
            routerInstance.getRouteForPath(window.location.pathname) ||
            routerInstance.getRoutes().BASIC),
        new URLSearchParams(window.location.search), true);
  });

  // TODO(dpapad): Change to 'get routes() {}' in export when we fix a bug in
  // ChromePass that limits the syntax of what can be returned from cr.define().
  const routes = settings.Router.getInstance().getRoutes();

  return {
    buildRouterForTesting: buildRouter,
    routes: routes,
  };
});
