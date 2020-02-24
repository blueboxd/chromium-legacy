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
   * @param {string} setting Value from settings.ContentSetting.
   * @param {string} enabled Non-block label ('feature X not allowed').
   * @param {string} disabled Block label (likely just, 'Blocked').
   * @param {?string} other Tristate value (maybe, 'session only').
   * @private
   */
  function defaultSettingLabel(setting, enabled, disabled, other) {
    if (setting == settings.ContentSetting.BLOCK) {
      return disabled;
    }
    if (setting == settings.ContentSetting.ALLOW) {
      return enabled;
    }
    return other || enabled;
  }

  /**
   * @typedef{{
   *   route: string,
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

  /**
   * @return {!Map<!settings.ContentSettingsTypes, !settings.CategoryListItem>}
   */
  function buildCategoryItemMap() {
    const Id = settings.ContentSettingsTypes;

    // The following list is ordered alphabetically by |id|. The order in which
    // these appear in the UI is determined elsewhere in this file.
    const categoryList = [
      {
        route: 'SITE_SETTINGS_ADS',
        id: Id.ADS,
        label: 'siteSettingsAds',
        icon: 'settings:ads',
        enabledLabel: 'siteSettingsAllowed',
        disabledLabel: 'siteSettingsAdsBlock',
        shouldShow: () =>
            loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter'),
      },
      {
        route: 'SITE_SETTINGS_AR',
        id: Id.AR,
        label: 'siteSettingsAr',
        icon: 'settings:vr-headset',
        enabledLabel: 'siteSettingsArAsk',
        disabledLabel: 'siteSettingsArBlock',
        shouldShow: () => loadTimeData.getBoolean('enableWebXrContentSetting'),
      },
      {
        route: 'SITE_SETTINGS_AUTOMATIC_DOWNLOADS',
        id: Id.AUTOMATIC_DOWNLOADS,
        label: 'siteSettingsAutomaticDownloads',
        icon: 'cr:file-download',
        enabledLabel: 'siteSettingsAutoDownloadAsk',
        disabledLabel: 'siteSettingsAutoDownloadBlock',
      },
      {
        route: 'SITE_SETTINGS_BACKGROUND_SYNC',
        id: Id.BACKGROUND_SYNC,
        label: 'siteSettingsBackgroundSync',
        icon: 'cr:sync',
        enabledLabel: 'siteSettingsAllowRecentlyClosedSites',
        disabledLabel: 'siteSettingsBackgroundSyncBlocked',
      },
      {
        route: 'SITE_SETTINGS_BLUETOOTH_SCANNING',
        id: Id.BLUETOOTH_SCANNING,
        label: 'siteSettingsBluetoothScanning',
        icon: 'settings:bluetooth-scanning',
        enabledLabel: 'siteSettingsBluetoothScanningAsk',
        disabledLabel: 'siteSettingsBluetoothScanningBlock',
        shouldShow: () =>
            loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures'),
      },
      {
        route: 'SITE_SETTINGS_CAMERA',
        id: Id.CAMERA,
        label: 'siteSettingsCamera',
        icon: 'cr:videocam',
        enabledLabel: 'siteSettingsAskBeforeAccessing',
        disabledLabel: 'siteSettingsBlocked',
      },
      {
        route: 'SITE_SETTINGS_CLIPBOARD',
        id: Id.CLIPBOARD,
        label: 'siteSettingsClipboard',
        icon: 'settings:clipboard',
        enabledLabel: 'siteSettingsAskBeforeAccessing',
        disabledLabel: 'siteSettingsBlocked',
      },
      {
        route: 'SITE_SETTINGS_COOKIES',
        id: Id.COOKIES,
        label: 'siteSettingsCookies',
        icon: 'settings:cookie',
        enabledLabel: 'siteSettingsCookiesAllowed',
        disabledLabel: 'siteSettingsCookiesAllowed',
        otherLabel: 'deleteDataPostSession',
      },
      {
        route: 'SITE_SETTINGS_LOCATION',
        id: Id.GEOLOCATION,
        label: 'siteSettingsLocation',
        icon: 'cr:location-on',
        enabledLabel: 'siteSettingsAskBeforeAccessing',
        disabledLabel: 'siteSettingsBlocked',
      },
      {
        route: 'SITE_SETTINGS_HID_DEVICES',
        id: Id.HID_DEVICES,
        label: 'siteSettingsHidDevices',
        icon: 'settings:hid-device',
        enabledLabel: 'siteSettingsHidDevicesAsk',
        disabledLabel: 'siteSettingsHidDevicesBlock',
        shouldShow: () =>
            loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures'),
      },
      {
        route: 'SITE_SETTINGS_IMAGES',
        id: Id.IMAGES,
        label: 'siteSettingsImages',
        icon: 'settings:photo',
        enabledLabel: 'siteSettingsShowAll',
        disabledLabel: 'siteSettingsDontShowImages',
      },
      {
        route: 'SITE_SETTINGS_JAVASCRIPT',
        id: Id.JAVASCRIPT,
        label: 'siteSettingsJavascript',
        icon: 'settings:code',
        enabledLabel: 'siteSettingsAllowed',
        disabledLabel: 'siteSettingsBlocked',
      },
      {
        route: 'SITE_SETTINGS_MICROPHONE',
        id: Id.MIC,
        label: 'siteSettingsMic',
        icon: 'cr:mic',
        enabledLabel: 'siteSettingsAskBeforeAccessing',
        disabledLabel: 'siteSettingsBlocked',
      },
      {
        route: 'SITE_SETTINGS_MIDI_DEVICES',
        id: Id.MIDI_DEVICES,
        label: 'siteSettingsMidiDevices',
        icon: 'settings:midi',
        enabledLabel: 'siteSettingsMidiDevicesAsk',
        disabledLabel: 'siteSettingsMidiDevicesBlock',
      },
      {
        route: 'SITE_SETTINGS_MIXEDSCRIPT',
        id: Id.MIXEDSCRIPT,
        label: 'siteSettingsInsecureContent',
        icon: 'settings:insecure-content',
        disabledLabel: 'siteSettingsInsecureContentBlock',
        shouldShow: () =>
            loadTimeData.getBoolean('enableInsecureContentContentSetting'),
      },
      {
        route: 'SITE_SETTINGS_NATIVE_FILE_SYSTEM_WRITE',
        id: Id.NATIVE_FILE_SYSTEM_WRITE,
        label: 'siteSettingsNativeFileSystemWrite',
        icon: 'settings:save-original',
        enabledLabel: 'siteSettingsNativeFileSystemWriteAsk',
        disabledLabel: 'siteSettingsNativeFileSystemWriteBlock',
        shouldShow: () => loadTimeData.getBoolean(
            'enableNativeFileSystemWriteContentSetting'),
      },
      {
        route: 'SITE_SETTINGS_NOTIFICATIONS',
        id: Id.NOTIFICATIONS,
        label: 'siteSettingsNotifications',
        icon: 'settings:notifications',
        enabledLabel: 'siteSettingsAskBeforeSending',
        disabledLabel: 'siteSettingsBlocked',
      },
      {
        route: 'SITE_SETTINGS_PAYMENT_HANDLER',
        id: Id.PAYMENT_HANDLER,
        label: 'siteSettingsPaymentHandler',
        icon: 'settings:bluetooth-scanning',
        enabledLabel: 'siteSettingsPaymentHandlerAllow',
        disabledLabel: 'siteSettingsPaymentHandlerBlock',
        shouldShow: () =>
            loadTimeData.getBoolean('enablePaymentHandlerContentSetting'),
      },
      {
        route: 'SITE_SETTINGS_PDF_DOCUMENTS',
        id: 'pdfDocuments',
        label: 'siteSettingsPdfDocuments',
        icon: 'settings:pdf',
      },
      {
        route: 'SITE_SETTINGS_FLASH',
        id: Id.PLUGINS,
        label: 'siteSettingsFlash',
        icon: 'cr:extension',
        enabledLabel: 'siteSettingsFlashAskFirst',
        disabledLabel: 'siteSettingsFlashBlock',
      },
      {
        route: 'SITE_SETTINGS_POPUPS',
        id: Id.POPUPS,
        label: 'siteSettingsPopups',
        icon: 'cr:open-in-new',
        enabledLabel: 'siteSettingsAllowed',
        disabledLabel: 'siteSettingsBlocked',
      },
      // <if expr="chromeos">
      {
        route: 'SITE_SETTINGS_PROTECTED_CONTENT',
        id: Id.PROTECTED_CONTENT,
        label: 'siteSettingsProtectedContent',
        icon: 'settings:protected-content',
      },
      // </if>
      {
        route: 'SITE_SETTINGS_HANDLERS',
        id: Id.PROTOCOL_HANDLERS,
        label: 'siteSettingsHandlers',
        icon: 'settings:protocol-handler',
        enabledLabel: 'siteSettingsHandlersAsk',
        disabledLabel: 'siteSettingsHandlersBlocked',
        shouldShow: () => !loadTimeData.getBoolean('isGuest'),
      },
      {
        route: 'SITE_SETTINGS_SENSORS',
        id: Id.SENSORS,
        label: 'siteSettingsSensors',
        icon: 'settings:sensors',
        enabledLabel: 'siteSettingsSensorsAllow',
        disabledLabel: 'siteSettingsSensorsBlock',
      },
      {
        route: 'SITE_SETTINGS_SERIAL_PORTS',
        id: Id.SERIAL_PORTS,
        label: 'siteSettingsSerialPorts',
        icon: 'settings:serial-port',
        enabledLabel: 'siteSettingsSerialPortsAsk',
        disabledLabel: 'siteSettingsSerialPortsBlock',
      },
      {
        route: 'SITE_SETTINGS_SOUND',
        id: Id.SOUND,
        label: 'siteSettingsSound',
        icon: 'settings:volume-up',
        enabledLabel: 'siteSettingsSoundAllow',
        disabledLabel: 'siteSettingsSoundBlock',
      },
      {
        route: 'SITE_SETTINGS_UNSANDBOXED_PLUGINS',
        id: Id.UNSANDBOXED_PLUGINS,
        label: 'siteSettingsUnsandboxedPlugins',
        icon: 'cr:extension',
        enabledLabel: 'siteSettingsUnsandboxedPluginsAsk',
        disabledLabel: 'siteSettingsUnsandboxedPluginsBlock',
      },
      {
        route: 'SITE_SETTINGS_USB_DEVICES',
        id: Id.USB_DEVICES,
        label: 'siteSettingsUsbDevices',
        icon: 'settings:usb',
        enabledLabel: 'siteSettingsUsbDevicesAsk',
        disabledLabel: 'siteSettingsUsbDevicesBlock',
      },
      {
        route: 'SITE_SETTINGS_VR',
        id: Id.VR,
        label: 'siteSettingsVr',
        icon: 'settings:vr-headset',
        enabledLabel: 'siteSettingsVrAsk',
        disabledLabel: 'siteSettingsVrBlock',
        shouldShow: () => loadTimeData.getBoolean('enableWebXrContentSetting'),
      },
      {
        route: 'SITE_SETTINGS_ZOOM_LEVELS',
        id: Id.ZOOM_LEVELS,
        label: 'siteSettingsZoomLevels',
        icon: 'settings:zoom-in',
      },
    ];

    return new Map(categoryList.map(item => [item.id, item]));
  }

  Polymer({
    is: 'settings-site-settings-page',

    behaviors: [
      SiteSettingsBehavior, WebUIListenerBehavior, I18nBehavior,
      settings.RouteObserverBehavior
    ],

    properties: {
      /** @private {!Array<!settings.CategoryListItem>} */
      categoryList_: {
        type: Array,
        value: function() {
          const Id = settings.ContentSettingsTypes;
          // Ordered according to how these should appear in the UI.
          const orderedIdList = [
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
          ];

          const map = buildCategoryItemMap();
          const orderedList = [];
          for (const id of orderedIdList) {
            const item = map.get(id);
            if (item.shouldShow === undefined || item.shouldShow()) {
              orderedList.push(item);
            }
          }
          return orderedList;
        },
      },

      /** @type {!Map<string, (string|Function)>} */
      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },

      /** @private */
      privacySettingsRedesignEnabled_: {
        type: Boolean,
        value: loadTimeData.getBoolean('privacySettingsRedesignEnabled'),
      },

      /* @private */
      noRecentSitePermissions_: {
        type: Boolean,
      },
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

      // Populate the |focusConfig| map of the parent <settings-animated-pages>
      // element, with additional entries that correspond to subpage trigger
      // elements residing in this element's Shadow DOM.

      // Register the allSites route manually, since it is not part of the
      // |categoryList_|.
      /** @type {!Array<{id: string, route: string}>} */ const list =
          this.categoryList_.slice();
      list.push({id: 'allSites', route: 'SITE_SETTINGS_ALL'});

      for (const item of list) {
        const route = settings.routes[item.route];
        this.focusConfig.set(route.path, () => this.async(() => {
          cr.ui.focusWithoutInk(assert(this.$$(`#${item.id}`)));
        }));
      }
    },

    /** @override */
    ready() {
      this.metricsBrowserProxy_ =
          settings.MetricsBrowserProxyImpl.getInstance();

      for (const item of this.categoryList_) {
        // Default labels are not applicable to ZOOM_LEVELS or PDF.
        if (item.id === settings.ContentSettingsTypes.ZOOM_LEVELS ||
            item.id === 'pdfDocuments') {
          continue;
        }

        this.refreshDefaultValueLabel_(item.id);
      }

      this.addWebUIListener(
          'contentSettingCategoryChanged',
          this.refreshDefaultValueLabel_.bind(this));

      // The protocol handlers have a separate enabled/disabled notifier.
      this.addWebUIListener('setHandlersEnabled', enabled => {
        this.updateDefaultValueLabel_(
            settings.ContentSettingsTypes.PROTOCOL_HANDLERS,
            enabled ? settings.ContentSetting.ALLOW :
                      settings.ContentSetting.BLOCK);
      });
      this.browserProxy.observeProtocolHandlersEnabledState();
    },

    /**
     * @param {!settings.ContentSettingsTypes} category The category to refresh
     *     (fetch current value + update UI)
     * @private
     */
    refreshDefaultValueLabel_(category) {
      this.browserProxy.getDefaultValueForContentType(category).then(
          defaultValue => {
            this.updateDefaultValueLabel_(category, defaultValue.setting);
          });
    },

    /**
     * Updates the DOM for the given |category| to display a label that
     * corresponds to the given |setting|.
     * @param {!settings.ContentSettingsTypes} category
     * @param {!settings.ContentSetting} setting
     * @private
     */
    updateDefaultValueLabel_(category, setting) {
      if (!this.isConnected) {
        // Do nothing if the element has been detached from the DOM (happens in
        // tests).
        return;
      }

      const element = this.$$(`#${category}`);
      const index = this.$$('dom-repeat').indexForElement(element);
      const dataItem = this.categoryList_[index];
      const subLabel = defaultSettingLabel(
          setting,
          dataItem.enabledLabel ? this.i18n(dataItem.enabledLabel) : '',
          dataItem.disabledLabel ? this.i18n(dataItem.disabledLabel) : '',
          dataItem.otherLabel ? this.i18n(dataItem.otherLabel) : null);
      this.set(`categoryList_.${index}.subLabel`, subLabel);
    },

    /**
     * Reload the site recent site permission list when the page is navigated
     * to.
     * settings.RouteObserverBehavior
     * @param {!settings.Route} currentRoute
     * @protected
     */
    currentRouteChanged(currentRoute) {
      if (currentRoute == settings.routes.SITE_SETTINGS &&
          this.privacySettingsRedesignEnabled_) {
        // Needs to be async to await the surrounding dom-if, should be removed
        // when the dom-if is removed.
        this.async(() => {
          this.$$('#recentSitePermissions').populateList();
        });
      }
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

    /**
     * Navigate to the route specified in the event dataset.
     * @param {!Event} event The tap event.
     * @private
     */
    onTapNavigate_(event) {
      const dataSet =
          /** @type {{route: string}} */ (event.currentTarget.dataset);
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          settings.SettingsPageInteractions['PRIVACY_' + dataSet.route]);
      settings.Router.getInstance().navigateTo(settings.routes[dataSet.route]);
    },
  });

  // #cr_define_end
  return {CategoryListItem, defaultSettingLabel};
});
