// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * Add all of the child routes that originate from the privacy route,
   * regardless of whether the privacy section under basic or advanced.
   * @param {!SettingsRoutes} r
   */
  function addPrivacyChildRoutes(r) {
    r.CERTIFICATES = r.PRIVACY.createChild('/certificates');
    r.SITE_SETTINGS = r.PRIVACY.createChild('/content');
    if (loadTimeData.getBoolean('enableSecurityKeysSubpage')) {
      r.SECURITY_KEYS = r.PRIVACY.createChild('/securityKeys');
    }
    if (loadTimeData.getBoolean('privacySettingsRedesignEnabled')) {
      r.SECURITY = r.PRIVACY.createChild('/security');
    }

    r.SITE_SETTINGS_ALL = r.SITE_SETTINGS.createChild('all');
    r.SITE_SETTINGS_SITE_DETAILS =
        r.SITE_SETTINGS_ALL.createChild('/content/siteDetails');

    r.SITE_SETTINGS_HANDLERS = r.SITE_SETTINGS.createChild('/handlers');

    // TODO(tommycli): Find a way to refactor these repetitive category
    // routes.
    r.SITE_SETTINGS_ADS = r.SITE_SETTINGS.createChild('ads');
    if (loadTimeData.getBoolean('enableWebXrContentSetting')) {
      r.SITE_SETTINGS_AR = r.SITE_SETTINGS.createChild('ar');
    }
    r.SITE_SETTINGS_AUTOMATIC_DOWNLOADS =
        r.SITE_SETTINGS.createChild('automaticDownloads');
    r.SITE_SETTINGS_BACKGROUND_SYNC =
        r.SITE_SETTINGS.createChild('backgroundSync');
    r.SITE_SETTINGS_CAMERA = r.SITE_SETTINGS.createChild('camera');
    r.SITE_SETTINGS_CLIPBOARD = r.SITE_SETTINGS.createChild('clipboard');
    r.SITE_SETTINGS_COOKIES = r.SITE_SETTINGS.createChild('cookies');
    r.SITE_SETTINGS_SITE_DATA =
        r.SITE_SETTINGS_COOKIES.createChild('/siteData');
    r.SITE_SETTINGS_DATA_DETAILS =
        r.SITE_SETTINGS_SITE_DATA.createChild('/cookies/detail');
    r.SITE_SETTINGS_IMAGES = r.SITE_SETTINGS.createChild('images');
    if (loadTimeData.getBoolean('enableInsecureContentContentSetting')) {
      r.SITE_SETTINGS_MIXEDSCRIPT =
          r.SITE_SETTINGS.createChild('insecureContent');
    }
    r.SITE_SETTINGS_JAVASCRIPT = r.SITE_SETTINGS.createChild('javascript');
    r.SITE_SETTINGS_SOUND = r.SITE_SETTINGS.createChild('sound');
    r.SITE_SETTINGS_SENSORS = r.SITE_SETTINGS.createChild('sensors');
    r.SITE_SETTINGS_LOCATION = r.SITE_SETTINGS.createChild('location');
    r.SITE_SETTINGS_MICROPHONE = r.SITE_SETTINGS.createChild('microphone');
    r.SITE_SETTINGS_NOTIFICATIONS =
        r.SITE_SETTINGS.createChild('notifications');
    r.SITE_SETTINGS_FLASH = r.SITE_SETTINGS.createChild('flash');
    r.SITE_SETTINGS_POPUPS = r.SITE_SETTINGS.createChild('popups');
    r.SITE_SETTINGS_UNSANDBOXED_PLUGINS =
        r.SITE_SETTINGS.createChild('unsandboxedPlugins');
    r.SITE_SETTINGS_MIDI_DEVICES = r.SITE_SETTINGS.createChild('midiDevices');
    r.SITE_SETTINGS_USB_DEVICES = r.SITE_SETTINGS.createChild('usbDevices');
    r.SITE_SETTINGS_SERIAL_PORTS = r.SITE_SETTINGS.createChild('serialPorts');
    r.SITE_SETTINGS_ZOOM_LEVELS = r.SITE_SETTINGS.createChild('zoomLevels');
    r.SITE_SETTINGS_PDF_DOCUMENTS = r.SITE_SETTINGS.createChild('pdfDocuments');
    r.SITE_SETTINGS_PROTECTED_CONTENT =
        r.SITE_SETTINGS.createChild('protectedContent');
    if (loadTimeData.getBoolean('enablePaymentHandlerContentSetting')) {
      r.SITE_SETTINGS_PAYMENT_HANDLER =
          r.SITE_SETTINGS.createChild('paymentHandler');
    }
    if (loadTimeData.getBoolean('enableWebXrContentSetting')) {
      r.SITE_SETTINGS_VR = r.SITE_SETTINGS.createChild('vr');
    }
    if (loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures')) {
      r.SITE_SETTINGS_BLUETOOTH_SCANNING =
          r.SITE_SETTINGS.createChild('bluetoothScanning');
      r.SITE_SETTINGS_HID_DEVICES = r.SITE_SETTINGS.createChild('hidDevices');
    }
    if (loadTimeData.getBoolean('enableNativeFileSystemWriteContentSetting')) {
      r.SITE_SETTINGS_NATIVE_FILE_SYSTEM_WRITE =
          r.SITE_SETTINGS.createChild('filesystem');
    }
  }

  /**
   * Adds Route objects for each path.
   * @return {!SettingsRoutes}
   */
  function createBrowserSettingsRoutes() {
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

    const pageVisibility = settings.pageVisibility || {};

    // <if expr="not chromeos">
    r.IMPORT_DATA = r.BASIC.createChild('/importData');
    r.IMPORT_DATA.isNavigableDialog = true;

    if (pageVisibility.people !== false) {
      r.MANAGE_PROFILE = r.PEOPLE.createChild('/manageProfile');
    }
    // </if>

    if (pageVisibility.appearance !== false) {
      r.APPEARANCE = r.BASIC.createSection('/appearance', 'appearance');
      r.FONTS = r.APPEARANCE.createChild('/fonts');
    }

    if (pageVisibility.autofill !== false) {
      r.AUTOFILL = r.BASIC.createSection('/autofill', 'autofill');
      r.PASSWORDS = r.AUTOFILL.createChild('/passwords');
      r.PAYMENTS = r.AUTOFILL.createChild('/payments');
      r.ADDRESSES = r.AUTOFILL.createChild('/addresses');
    }

    r.CLEAR_BROWSER_DATA = r.BASIC.createChild('/clearBrowserData');
    r.CLEAR_BROWSER_DATA.isNavigableDialog = true;

    if (pageVisibility.privacy !== false) {
      r.PRIVACY = r.BASIC.createSection('/privacy', 'privacy');
      addPrivacyChildRoutes(r);
    }

    if (pageVisibility.defaultBrowser !== false) {
      r.DEFAULT_BROWSER =
          r.BASIC.createSection('/defaultBrowser', 'defaultBrowser');
    }

    r.SEARCH_ENGINES = r.SEARCH.createChild('/searchEngines');

    if (pageVisibility.onStartup !== false) {
      r.ON_STARTUP = r.BASIC.createSection('/onStartup', 'onStartup');
      r.STARTUP_PAGES = r.ON_STARTUP.createChild('/startupPages');
    }

    // Advanced Routes
    if (pageVisibility.advancedSettings !== false) {
      r.ADVANCED = new settings.Route('/advanced');

      r.LANGUAGES = r.ADVANCED.createSection('/languages', 'languages');
      // <if expr="not is_macosx">
      r.EDIT_DICTIONARY = r.LANGUAGES.createChild('/editDictionary');
      // </if>

      if (pageVisibility.downloads !== false) {
        r.DOWNLOADS = r.ADVANCED.createSection('/downloads', 'downloads');
      }

      r.PRINTING = r.ADVANCED.createSection('/printing', 'printing');
      r.CLOUD_PRINTERS = r.PRINTING.createChild('/cloudPrinters');

      r.ACCESSIBILITY = r.ADVANCED.createSection('/accessibility', 'a11y');

      // <if expr="chromeos or is_linux">
      if (loadTimeData.getBoolean('enableCaptionSettings')) {
        r.CAPTIONS = r.ACCESSIBILITY.createChild('/captions');
      }
      // </if>

      // <if expr="is_win">
      if (loadTimeData.getBoolean('enableCaptionSettings') &&
          !loadTimeData.getBoolean('isWindows10OrNewer')) {
        r.CAPTIONS = r.ACCESSIBILITY.createChild('/captions');
      }
      // </if>

      // <if expr="not chromeos">
      r.SYSTEM = r.ADVANCED.createSection('/system', 'system');
      // </if>

      if (pageVisibility.reset !== false) {
        r.RESET = r.ADVANCED.createSection('/reset', 'reset');
        r.RESET_DIALOG = r.ADVANCED.createChild('/resetProfileSettings');
        r.RESET_DIALOG.isNavigableDialog = true;
        r.TRIGGERED_RESET_DIALOG =
            r.ADVANCED.createChild('/triggeredResetProfileSettings');
        r.TRIGGERED_RESET_DIALOG.isNavigableDialog = true;
        // <if expr="_google_chrome and is_win">
        r.CHROME_CLEANUP = r.RESET.createChild('/cleanup');
        if (loadTimeData.getBoolean('showIncompatibleApplications')) {
          r.INCOMPATIBLE_APPLICATIONS =
              r.RESET.createChild('/incompatibleApplications');
        }
        // </if>
      }
    }
    return r;
  }

  /**
   * @return {!settings.Router} A router with the browser settings routes.
   */
  function buildRouter() {
    return new settings.Router(createBrowserSettingsRoutes());
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
