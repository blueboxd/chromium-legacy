// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-recent-site-permissions',

  behaviors: [
    SiteSettingsBehavior,
    WebUIListenerBehavior,
    I18nBehavior,
  ],

  properties: {
    /** @type boolean */
    noRecentPermissions: {
      type: Boolean,
      computed: 'noRecentPermissions_(recentSitePermissionsList_)',
      notify: true,
    },

    /**
     * List of recent site permissions grouped by source.
     * @type {!Array<RecentSitePermissions>}
     * @private
     */
    recentSitePermissionsList_: {
      type: Array,
      value: () => [],
    },
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'onIncognitoStatusChanged', this.onIncognitoStatusChanged_.bind(this));
    this.browserProxy.updateIncognitoStatus();
  },

  /**
   * Perform internationalization for the given content settings type.
   * @return {string} The localised content setting type string
   * @private
   */
  getI18nContentTypeString_(contentSettingsType) {
    switch (contentSettingsType) {
      case 'cookies':
        return this.i18n('siteSettingsCookies');
      case 'images':
        return this.i18n('siteSettingsImages');
      case 'javascript':
        return this.i18n('siteSettingsJavascript');
      case 'sound':
        return this.i18n('siteSettingsSound');
      case 'popups':
        return this.i18n('siteSettingsPopups');
      case 'location':
        return this.i18n('siteSettingsLocation');
      case 'notifications':
        return this.i18n('siteSettingsNotifications');
      case 'media-stream-mic':
        return this.i18n('siteSettingsMic');
      case 'media-stream-camera':
        return this.i18n('siteSettingsCamera');
      case 'register-protocol-handler':
        return this.i18n('siteSettingsHandlers');
      case 'ppapi-broker':
        return this.i18n('siteSettingsUnsandboxedPlugins');
      case 'multiple-automatic-downloads':
        return this.i18n('siteSettingsAutomaticDownloads');
      case 'background-sync':
        return this.i18n('siteSettingsBackgroundSync');
      case 'midi-sysex':
        return this.i18n('siteSettingsMidiDevices');
      case 'usb-devices':
        return this.i18n('siteSettingsUsbDevices');
      case 'serial-ports':
        return this.i18n('siteSettingsSerialPorts');
      case 'zoom-levels':
        return this.i18n('siteSettingsZoomLevels');
      case 'protected-content':
        return this.i18n('siteSettingsProtectedContent');
      case 'ads':
        return this.i18n('siteSettingsAds');
      case 'clipboard':
        return this.i18n('siteSettingsClipboard');
      case 'sensors':
        return this.i18n('siteSettingsSensors');
      case 'payment-handler':
        return this.i18n('siteSettingsPaymentHandler');
      case 'mixed-script':
        return this.i18n('siteSettingsInsecureContent');
      case 'bluetooth-scanning':
        return this.i18n('siteSettingsBluetoothScanning');
      case 'native-file-system-write':
        return this.i18n('siteSettingsNativeFileSystemWrite');
      case 'hid-devices':
        return this.i18n('siteSettingsHidDevices');
      case 'ar':
        return this.i18n('siteSettingsAr');
      case 'vr':
        return this.i18n('siteSettingsVr');
      default:
        return '';
    }
  },

  /**
   * Return the display string representing a permission change.
   * @param {!RawSiteException} rawSiteException
   * @param {boolean} sentenceStart Whether the returned string will start a
   *     sentence.
   * @return {string} The string representing the permission change.
   * @private
   */
  getI18nPermissionChangeString_({setting, source, type}, sentenceStart) {
    let change;
    if (setting === settings.ContentSetting.ALLOW) {
      change = 'Allowed';
    } else if (setting === settings.ContentSetting.BLOCK) {
      if (source === settings.SiteSettingSource.EMBARGO) {
        change = 'Autoblocked';
      } else if (source === settings.SiteSettingSource.PREFERENCE) {
        change = 'Blocked';
      } else {
        return '';
      }
    }
    const suffix = sentenceStart ? 'SentenceStart' : '';
    const msgId = `recentPermissionChange${change}${suffix}`;
    return this.i18n(msgId, this.getI18nContentTypeString_(type));
  },

  /**
   * Returns a user-friendly name for the origin a set of recent permissions
   * is associated with.
   * @param {!RecentSitePermissions} recentSitePermissions
   * @return {string} The user-friendly name.
   * @private
   */
  getDisplayName_(recentSitePermissions) {
    const url = this.toUrl(recentSitePermissions.origin);
    return url.host;
  },

  /**
   * Returns the site scheme for the origin of a set of recent permissions.
   * @param {!RecentSitePermissions} recentSitePermissions
   * @return {string} The site scheme.
   * @private
   */
  getSiteScheme_({origin}) {
    const url = this.toUrl(origin);
    const scheme = url.protocol.slice(0, -1);
    return scheme === 'https' ? '' : scheme;
  },

  /**
   * Returns the display text which describes the set of recent permissions.
   * @param {!RecentSitePermissions} recentSitePermissions
   * @return {string} The display string for set of site permissions.
   * @private
   */
  getPermissionsText_({recentPermissions, incognito}) {
    const displayStrings = recentPermissions.map(
        (rp, i) => this.getI18nPermissionChangeString_(rp, i === 0));

    if (recentPermissions.length === 1 && !incognito) {
      return displayStrings[0];
    }

    const itemsPart = [
      'OneItem',
      'TwoItems',
      'ThreeItems',
      'OverThreeItems',
    ][Math.min(recentPermissions.length, 4) - 1];
    const suffix = incognito ? 'Incognito' : '';
    const i18nStringID = `recentPermissions${itemsPart}${suffix}`;

    return this.i18n(i18nStringID, ...displayStrings);
  },

  /**
   * Returns the correct class to apply depending on this recent site
   * permissions entry based on the index.
   * @param {number} index
   * @return {string} The CSS class corresponding to the provided index
   * @private
   */
  getClassForIndex_(index) {
    return index === 0 ? 'first' : '';
  },

  /**
   * Returns true if there are no recent site permissions to display
   * @return {boolean}
   * @private
   */
  noRecentPermissions_() {
    return this.recentSitePermissionsList_.length === 0;
  },

  /**
   * Called for when incognito is enabled or disabled. Only called on change
   * (opening N incognito windows only fires one message). Another message is
   * sent when the *last* incognito window closes.
   * @private
   */
  onIncognitoStatusChanged_(hasIncognito) {
    // We're only interested in the case where we transition out of incognito
    // and we are currently displaying an incognito entry.
    if (hasIncognito === false &&
        this.recentSitePermissionsList_.some(p => p.incognito)) {
      this.populateList();
    }
  },

  /**
   * A handler for selecting a recent site permissions entry.
   * @param {!{model: !{index: number}}} e
   * @private
   */
  onRecentSitePermissionClick_(e) {
    const origin = this.recentSitePermissionsList_[e.model.index].origin;
    settings.Router.getInstance().navigateTo(
        settings.routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams({site: origin}));
    this.browserProxy.recordAction(settings.AllSitesAction.ENTER_SITE_DETAILS);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onShowIncognitoTooltip_(e) {
    e.stopPropagation();

    const target = e.target;
    const tooltip = this.$.tooltip;
    tooltip.target = target;
    /** @type {{updatePosition: Function}} */ (tooltip).updatePosition();
    const hide = () => {
      /** @type {{hide: Function}} */ (tooltip).hide();
      target.removeEventListener('mouseleave', hide);
      target.removeEventListener('blur', hide);
      target.removeEventListener('tap', hide);
      tooltip.removeEventListener('mouseenter', hide);
    };
    target.addEventListener('mouseleave', hide);
    target.addEventListener('blur', hide);
    target.addEventListener('tap', hide);
    tooltip.addEventListener('mouseenter', hide);

    tooltip.show();
  },

  /**
   * Retrieve the list of recently changed permissions and implicitly trigger
   * the update of the display list.
   * @private
   */
  async populateList() {
    this.recentSitePermissionsList_ =
        await this.browserProxy.getRecentSitePermissions(
            this.getCategoryList(), 3);
  },
});
