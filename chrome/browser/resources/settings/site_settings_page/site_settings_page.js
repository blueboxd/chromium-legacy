// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-settings-page' is the settings page containing privacy and
 * security site settings.
 */

cr.define('settings', function() {
  /**
   * @typedef{{
   *   route: !settings.Route,
   *   id: settings.ContentSettingsTypes,
   *   label: string,
   *   icon: (string|undefined),
   *   enabledLabel: (string|undefined),
   *   disabledLabel: (string|undefined),
   *   otherLabel: (string|undefined),
   *   shouldShow: function():boolean,
   * }}
   */
  let CategoryListItem;

  const Id = settings.ContentSettingsTypes;

  /**
   * @type {?Map<!settings.ContentSettingsTypes, !settings.CategoryListItem>}
   */
  let categoryItemMap = null;

  /**
   * @return {!Map<!settings.ContentSettingsTypes, !settings.CategoryListItem>}
   */
  function getCategoryItemMap() {
    if (categoryItemMap !== null) {
      return categoryItemMap;
    }

    // The following list is ordered alphabetically by |id|. The order in which
    // these appear in the UI is determined elsewhere in this file.
    const categoryList = [
      {
        route: settings.routes.SITE_SETTINGS_ADS,
        id: Id.ADS,
        label: 'siteSettingsAds',
        icon: 'settings:ads',
        enabledLabel: 'siteSettingsAllowed',
        disabledLabel: 'siteSettingsAdsBlock',
        shouldShow: () =>
            loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter'),
      },
      {
        route: settings.routes.SITE_SETTINGS_AR,
        id: Id.AR,
        label: 'siteSettingsAr',
        icon: 'settings:vr-headset',
        enabledLabel: 'siteSettingsArAsk',
        disabledLabel: 'siteSettingsArBlock',
        shouldShow: () => loadTimeData.getBoolean('enableWebXrContentSetting'),
      },
      {
        route: settings.routes.SITE_SETTINGS_AUTOMATIC_DOWNLOADS,
        id: Id.AUTOMATIC_DOWNLOADS,
        label: 'siteSettingsAutomaticDownloads',
        icon: 'cr:file-download',
        enabledLabel: 'siteSettingsAutoDownloadAsk',
        disabledLabel: 'siteSettingsAutoDownloadBlock',
      },
      {
        route: settings.routes.SITE_SETTINGS_BACKGROUND_SYNC,
        id: Id.BACKGROUND_SYNC,
        label: 'siteSettingsBackgroundSync',
        icon: 'cr:sync',
        enabledLabel: 'siteSettingsAllowRecentlyClosedSites',
        disabledLabel: 'siteSettingsBackgroundSyncBlocked',
      },
      {
        route: settings.routes.SITE_SETTINGS_BLUETOOTH_DEVICES,
        id: Id.BLUETOOTH_DEVICES,
        label: 'siteSettingsBluetoothDevices',
        icon: 'settings:bluetooth',
        enabledLabel: 'siteSettingsBluetoothDevicesAsk',
        disabledLabel: 'siteSettingsBluetoothDevicesBlock',
        shouldShow: () =>
            loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend'),
      },
      {
        route: settings.routes.SITE_SETTINGS_BLUETOOTH_SCANNING,
        id: Id.BLUETOOTH_SCANNING,
        label: 'siteSettingsBluetoothScanning',
        icon: 'settings:bluetooth-scanning',
        enabledLabel: 'siteSettingsBluetoothScanningAsk',
        disabledLabel: 'siteSettingsBluetoothScanningBlock',
        shouldShow: () =>
            loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures'),
      },
      {
        route: settings.routes.SITE_SETTINGS_CAMERA,
        id: Id.CAMERA,
        label: 'siteSettingsCamera',
        icon: 'cr:videocam',
        enabledLabel: 'siteSettingsAskBeforeAccessing',
        disabledLabel: 'siteSettingsBlocked',
      },
      {
        route: settings.routes.SITE_SETTINGS_CLIPBOARD,
        id: Id.CLIPBOARD,
        label: 'siteSettingsClipboard',
        icon: 'settings:clipboard',
        enabledLabel: 'siteSettingsAskBeforeAccessing',
        disabledLabel: 'siteSettingsBlocked',
      },
      {
        route: (function() {
          return loadTimeData.getBoolean('privacySettingsRedesignEnabled') ?
              settings.routes.COOKIES :
              settings.routes.SITE_SETTINGS_COOKIES;
        })(),
        id: Id.COOKIES,
        label: 'siteSettingsCookies',
        icon: 'settings:cookie',
        enabledLabel: 'siteSettingsCookiesAllowed',
        disabledLabel: 'siteSettingsBlocked',
        otherLabel: 'deleteDataPostSession',
      },
      {
        route: settings.routes.SITE_SETTINGS_LOCATION,
        id: Id.GEOLOCATION,
        label: 'siteSettingsLocation',
        icon: 'cr:location-on',
        enabledLabel: 'siteSettingsAskBeforeAccessing',
        disabledLabel: 'siteSettingsBlocked',
      },
      {
        route: settings.routes.SITE_SETTINGS_HID_DEVICES,
        id: Id.HID_DEVICES,
        label: 'siteSettingsHidDevices',
        icon: 'settings:hid-device',
        enabledLabel: 'siteSettingsHidDevicesAsk',
        disabledLabel: 'siteSettingsHidDevicesBlock',
        shouldShow: () =>
            loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures'),
      },
      {
        route: settings.routes.SITE_SETTINGS_IMAGES,
        id: Id.IMAGES,
        label: 'siteSettingsImages',
        icon: 'settings:photo',
        enabledLabel: 'siteSettingsShowAll',
        disabledLabel: 'siteSettingsDontShowImages',
      },
      {
        route: settings.routes.SITE_SETTINGS_JAVASCRIPT,
        id: Id.JAVASCRIPT,
        label: 'siteSettingsJavascript',
        icon: 'settings:code',
        enabledLabel: 'siteSettingsAllowed',
        disabledLabel: 'siteSettingsBlocked',
      },
      {
        route: settings.routes.SITE_SETTINGS_MICROPHONE,
        id: Id.MIC,
        label: 'siteSettingsMic',
        icon: 'cr:mic',
        enabledLabel: 'siteSettingsAskBeforeAccessing',
        disabledLabel: 'siteSettingsBlocked',
      },
      {
        route: settings.routes.SITE_SETTINGS_MIDI_DEVICES,
        id: Id.MIDI_DEVICES,
        label: 'siteSettingsMidiDevices',
        icon: 'settings:midi',
        enabledLabel: 'siteSettingsMidiDevicesAsk',
        disabledLabel: 'siteSettingsMidiDevicesBlock',
      },
      {
        route: settings.routes.SITE_SETTINGS_MIXEDSCRIPT,
        id: Id.MIXEDSCRIPT,
        label: 'siteSettingsInsecureContent',
        icon: 'settings:insecure-content',
        disabledLabel: 'siteSettingsInsecureContentBlock',
        shouldShow: () =>
            loadTimeData.getBoolean('enableInsecureContentContentSetting'),
      },
      {
        route: settings.routes.SITE_SETTINGS_NATIVE_FILE_SYSTEM_WRITE,
        id: Id.NATIVE_FILE_SYSTEM_WRITE,
        label: 'siteSettingsNativeFileSystemWrite',
        icon: 'settings:save-original',
        enabledLabel: 'siteSettingsNativeFileSystemWriteAsk',
        disabledLabel: 'siteSettingsNativeFileSystemWriteBlock',
        shouldShow: () => loadTimeData.getBoolean(
            'enableNativeFileSystemWriteContentSetting'),
      },
      {
        route: settings.routes.SITE_SETTINGS_NOTIFICATIONS,
        id: Id.NOTIFICATIONS,
        label: 'siteSettingsNotifications',
        icon: 'settings:notifications',
        enabledLabel: 'siteSettingsAskBeforeSending',
        disabledLabel: 'siteSettingsBlocked',
      },
      {
        route: settings.routes.SITE_SETTINGS_PAYMENT_HANDLER,
        id: Id.PAYMENT_HANDLER,
        label: 'siteSettingsPaymentHandler',
        icon: 'settings:payment-handler',
        enabledLabel: 'siteSettingsPaymentHandlerAllow',
        disabledLabel: 'siteSettingsPaymentHandlerBlock',
        shouldShow: () =>
            loadTimeData.getBoolean('enablePaymentHandlerContentSetting'),
      },
      {
        route: settings.routes.SITE_SETTINGS_PDF_DOCUMENTS,
        id: 'pdfDocuments',
        label: 'siteSettingsPdfDocuments',
        icon: 'settings:pdf',
      },
      {
        route: settings.routes.SITE_SETTINGS_FLASH,
        id: Id.PLUGINS,
        label: 'siteSettingsFlash',
        icon: 'cr:extension',
        enabledLabel: 'siteSettingsFlashAskFirst',
        disabledLabel: 'siteSettingsFlashBlock',
      },
      {
        route: settings.routes.SITE_SETTINGS_POPUPS,
        id: Id.POPUPS,
        label: 'siteSettingsPopups',
        icon: 'cr:open-in-new',
        enabledLabel: 'siteSettingsAllowed',
        disabledLabel: 'siteSettingsBlocked',
      },
      // <if expr="chromeos">
      {
        route: settings.routes.SITE_SETTINGS_PROTECTED_CONTENT,
        id: Id.PROTECTED_CONTENT,
        label: 'siteSettingsProtectedContent',
        icon: 'settings:protected-content',
      },
      // </if>
      {
        route: settings.routes.SITE_SETTINGS_HANDLERS,
        id: Id.PROTOCOL_HANDLERS,
        label: 'siteSettingsHandlers',
        icon: 'settings:protocol-handler',
        enabledLabel: 'siteSettingsHandlersAsk',
        disabledLabel: 'siteSettingsHandlersBlocked',
        shouldShow: () => !loadTimeData.getBoolean('isGuest'),
      },
      {
        route: settings.routes.SITE_SETTINGS_SENSORS,
        id: Id.SENSORS,
        label: 'siteSettingsSensors',
        icon: 'settings:sensors',
        enabledLabel: 'siteSettingsSensorsAllow',
        disabledLabel: 'siteSettingsSensorsBlock',
      },
      {
        route: settings.routes.SITE_SETTINGS_SERIAL_PORTS,
        id: Id.SERIAL_PORTS,
        label: 'siteSettingsSerialPorts',
        icon: 'settings:serial-port',
        enabledLabel: 'siteSettingsSerialPortsAsk',
        disabledLabel: 'siteSettingsSerialPortsBlock',
      },
      {
        route: settings.routes.SITE_SETTINGS_SOUND,
        id: Id.SOUND,
        label: 'siteSettingsSound',
        icon: 'settings:volume-up',
        enabledLabel: 'siteSettingsSoundAllow',
        disabledLabel: 'siteSettingsSoundBlock',
      },
      {
        route: settings.routes.SITE_SETTINGS_UNSANDBOXED_PLUGINS,
        id: Id.UNSANDBOXED_PLUGINS,
        label: 'siteSettingsUnsandboxedPlugins',
        icon: 'cr:extension',
        enabledLabel: 'siteSettingsUnsandboxedPluginsAsk',
        disabledLabel: 'siteSettingsUnsandboxedPluginsBlock',
      },
      {
        route: settings.routes.SITE_SETTINGS_USB_DEVICES,
        id: Id.USB_DEVICES,
        label: 'siteSettingsUsbDevices',
        icon: 'settings:usb',
        enabledLabel: 'siteSettingsUsbDevicesAsk',
        disabledLabel: 'siteSettingsUsbDevicesBlock',
      },
      {
        route: settings.routes.SITE_SETTINGS_VR,
        id: Id.VR,
        label: 'siteSettingsVr',
        icon: 'settings:vr-headset',
        enabledLabel: 'siteSettingsVrAsk',
        disabledLabel: 'siteSettingsVrBlock',
        shouldShow: () => loadTimeData.getBoolean('enableWebXrContentSetting'),
      },
      {
        route: settings.routes.SITE_SETTINGS_ZOOM_LEVELS,
        id: Id.ZOOM_LEVELS,
        label: 'siteSettingsZoomLevels',
        icon: 'settings:zoom-in',
      },
    ];

    categoryItemMap = new Map(categoryList.map(item => [item.id, item]));
    return categoryItemMap;
  }

  /**
   * @param {!Array<!settings.ContentSettingsTypes>} orderedIdList
   * @return {!Array<!settings.CategoryListItem>}
   */
  function buildItemListFromIds(orderedIdList) {
    const map = getCategoryItemMap();
    const orderedList = [];
    for (const id of orderedIdList) {
      const item = map.get(id);
      if (item.shouldShow === undefined || item.shouldShow()) {
        orderedList.push(item);
      }
    }
    return orderedList;
  }

  Polymer({
    is: 'settings-site-settings-page',

    properties: {
      /**
       * @private {{
       *   all: (!Array<!settings.CategoryListItem>|undefined),
       *   permissionsBasic: (!Array<!settings.CategoryListItem>|undefined),
       *   permissionsAdvanced: (!Array<!settings.CategoryListItem>|undefined),
       *   contentBasic: (!Array<!settings.CategoryListItem>|undefined),
       *   contentAdvanced: (!Array<!settings.CategoryListItem>|undefined)
       * }}
       */
      lists_: {
        type: Object,
        value: function() {
          if (!loadTimeData.getBoolean('privacySettingsRedesignEnabled')) {
            return {
              all: buildItemListFromIds([
                Id.COOKIES,
                Id.GEOLOCATION,
                Id.CAMERA,
                Id.MIC,
                Id.SENSORS,
                Id.NOTIFICATIONS,
                Id.JAVASCRIPT,
                Id.PLUGINS,
                Id.IMAGES,
                Id.POPUPS,
                Id.ADS,
                Id.BACKGROUND_SYNC,
                Id.SOUND,
                Id.AUTOMATIC_DOWNLOADS,
                Id.UNSANDBOXED_PLUGINS,
                Id.PROTOCOL_HANDLERS,
                Id.MIDI_DEVICES,
                Id.ZOOM_LEVELS,
                Id.USB_DEVICES,
                Id.SERIAL_PORTS,
                Id.BLUETOOTH_DEVICES,
                Id.NATIVE_FILE_SYSTEM_WRITE,
                Id.HID_DEVICES,
                'pdfDocuments',
                // <if expr="chromeos">
                Id.PROTECTED_CONTENT,
                // </if>
                Id.CLIPBOARD,
                Id.PAYMENT_HANDLER,
                Id.MIXEDSCRIPT,
                Id.BLUETOOTH_SCANNING,
                Id.AR,
                Id.VR,
              ]),
            };
          }

          return {
            permissionsBasic: buildItemListFromIds([
              Id.GEOLOCATION,
              Id.CAMERA,
              Id.MIC,
              Id.NOTIFICATIONS,
              Id.BACKGROUND_SYNC,
            ]),
            permissionsAdvanced: buildItemListFromIds([
              Id.SENSORS,
              Id.AUTOMATIC_DOWNLOADS,
              Id.UNSANDBOXED_PLUGINS,
              Id.PROTOCOL_HANDLERS,
              Id.MIDI_DEVICES,
              Id.USB_DEVICES,
              Id.SERIAL_PORTS,
              Id.BLUETOOTH_DEVICES,
              Id.NATIVE_FILE_SYSTEM_WRITE,
              Id.HID_DEVICES,
              Id.CLIPBOARD,
              Id.PAYMENT_HANDLER,
              Id.BLUETOOTH_SCANNING,
              Id.AR,
              Id.VR,
            ]),
            contentBasic: buildItemListFromIds([
              Id.COOKIES,
              Id.JAVASCRIPT,
              Id.PLUGINS,
              Id.IMAGES,
              Id.POPUPS,
            ]),
            contentAdvanced: buildItemListFromIds([
              Id.SOUND,
              Id.ADS,
              Id.ZOOM_LEVELS,
              'pdfDocuments',
              // <if expr="chromeos">
              Id.PROTECTED_CONTENT,
              // </if>
              Id.MIXEDSCRIPT,
            ]),
          };
        }
      },

      /** @type {!Map<string, (string|Function)>} */
      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },

      /** @private */
      privacySettingsRedesignEnabled_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('privacySettingsRedesignEnabled');
        },
      },

      /** @private */
      permissionsExpanded_: Boolean,

      /** @private */
      contentExpanded_: Boolean,

      /* @private */
      noRecentSitePermissions_: Boolean,
    },

    /**
     * @param {!Map<string, string>} newConfig
     * @param {?Map<string, string>} oldConfig
     * @private
     */
    focusConfigChanged_(newConfig, oldConfig) {
      // focusConfig is set only once on the parent, so this observer should
      // only fire once.
      assert(!oldConfig);
      this.focusConfig.set(settings.routes.SITE_SETTINGS_ALL.path, () => {
        cr.ui.focusWithoutInk(assert(this.$$('#allSites')));
      });
    },

    /** @private */
    onSiteSettingsAllClick_() {
      settings.Router.getInstance().navigateTo(
          settings.routes.SITE_SETTINGS_ALL);
    },

    /**
     * @return {string} Class for the all site settings link
     * @private
     */
    getClassForSiteSettingsAllLink_() {
      return (this.privacySettingsRedesignEnabled_ &&
              !this.noRecentSitePermissions_) ?
          'hr' :
          '';
    },
  });

  // #cr_define_end
  return {CategoryListItem};
});
