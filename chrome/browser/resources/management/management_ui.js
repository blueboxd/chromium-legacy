// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('management', function() {
  /**
   * @typedef {{
   *   messageIds: !Array<string>,
   *   icon: string,
   * }}
   */
  let BrowserReportingData;

  Polymer({
    is: 'management-ui',

    behaviors: [
      I18nBehavior,
      WebUIListenerBehavior,
    ],

    properties: {
      /**
       * List of messages related to browser reporting.
       * @private {?Array<!management.BrowserReportingData>}
       */
      browserReportingInfo_: Array,

      /**
       * List of messages related to browser reporting.
       * @private {?Array<!management.Extension>}
       */
      extensions_: Array,

      // <if expr="chromeos">
      /**
       * List of messages related to device reporting.
       * @private {?Array<!management.DeviceReportingResponse>}
       */
      deviceReportingInfo_: Array,

      /**
       * Message stating if the Trust Roots are configured.
       * @private
       */
      localTrustRoots_: String,

      /** @private */
      customerLogo_: String,

      /** @private */
      managementOverview_: String,

      // </if>

      /** @private */
      subtitle_: String,

      // <if expr="not chromeos">
      /** @private */
      managementNoticeHtml_: String,
      // </if>

      /** @private */
      managed_: Boolean,

      /** @private */
      extensionReportingSubtitle_: String,
    },

    /** @private {?management.ManagementBrowserProxy} */
    browserProxy_: null,

    /** @override */
    attached() {
      document.documentElement.classList.remove('loading');
      this.browserProxy_ = management.ManagementBrowserProxyImpl.getInstance();
      this.updateManagedFields_();
      this.initBrowserReportingInfo_();

      this.addWebUIListener(
          'browser-reporting-info-updated',
          reportingInfo => this.onBrowserReportingInfoReceived_(reportingInfo));

      this.addWebUIListener('managed_data_changed', () => {
        this.updateManagedFields_();
      });

      this.getExtensions_();
      // <if expr="chromeos">
      this.getDeviceReportingInfo_();
      this.getLocalTrustRootsInfo_();
      // </if>
    },

    /** @private */
    initBrowserReportingInfo_() {
      this.browserProxy_.initBrowserReportingInfo().then(
          reportingInfo => this.onBrowserReportingInfoReceived_(reportingInfo));
    },

    /**
     * @param {!Array<!management.BrowserReportingResponse>} reportingInfo
     * @private
     */
    onBrowserReportingInfoReceived_(reportingInfo) {
      const reportingInfoMap = reportingInfo.reduce((info, response) => {
        info[response.reportingType] = info[response.reportingType] || {
          icon: this.getIconForReportingType_(response.reportingType),
          messageIds: []
        };
        info[response.reportingType].messageIds.push(response.messageId);
        return info;
      }, {});

      const reportingTypeOrder = {
        [management.ReportingType.SECURITY]: 1,
        [management.ReportingType.EXTENSIONS]: 2,
        [management.ReportingType.USER]: 3,
        [management.ReportingType.USER_ACTIVITY]: 4,
        [management.ReportingType.DEVICE]: 5,
      };

      this.browserReportingInfo_ =
          Object.keys(reportingInfoMap)
              .sort((a, b) => reportingTypeOrder[a] - reportingTypeOrder[b])
              .map(reportingType => reportingInfoMap[reportingType]);
    },

    /** @private */
    getExtensions_() {
      this.browserProxy_.getExtensions().then(extensions => {
        this.extensions_ = extensions;
      });
    },

    // <if expr="chromeos">
    /** @private */
    getLocalTrustRootsInfo_() {
      this.browserProxy_.getLocalTrustRootsInfo().then(trustRootsConfigured => {
        this.localTrustRoots_ = trustRootsConfigured ?
            loadTimeData.getString('managementTrustRootsConfigured') :
            '';
      });
    },

    /** @private */
    getDeviceReportingInfo_() {
      this.browserProxy_.getDeviceReportingInfo().then(reportingInfo => {
        this.deviceReportingInfo_ = reportingInfo;
      });
    },

    /**
     * @return {boolean} True of there are device reporting info to show.
     * @private
     */
    showDeviceReportingInfo_() {
      return !!this.deviceReportingInfo_ &&
          this.deviceReportingInfo_.length > 0;
    },

    /**
     * @param {management.DeviceReportingType} reportingType
     * @return {string} The associated icon.
     * @private
     */
    getIconForDeviceReportingType_(reportingType) {
      switch (reportingType) {
        case management.DeviceReportingType.SUPERVISED_USER:
          return 'management:supervised-user';
        case management.DeviceReportingType.DEVICE_ACTIVITY:
          return 'management:timelapse';
        case management.DeviceReportingType.STATISTIC:
          return 'management:bar-chart';
        case management.DeviceReportingType.DEVICE:
          return 'cr:computer';
        case management.DeviceReportingType.LOGS:
          return 'management:report';
        case management.DeviceReportingType.PRINT:
          return 'cr:print';
        case management.DeviceReportingType.CROSTINI:
          return 'management:linux';
        default:
          return 'cr:computer';
      }
    },
    // </if>

    /**
     * @return {boolean} True of there are browser reporting info to show.
     * @private
     */
    showBrowserReportingInfo_() {
      return !!this.browserReportingInfo_ &&
          this.browserReportingInfo_.length > 0;
    },

    /**
     * @return {boolean} True of there are extension reporting info to show.
     * @private
     */
    showExtensionReportingInfo_() {
      return !!this.extensions_ && this.extensions_.length > 0;
    },

    /**
     * @param {management.ReportingType} reportingType
     * @returns {string} The associated icon.
     * @private
     */
    getIconForReportingType_(reportingType) {
      switch (reportingType) {
        case management.ReportingType.SECURITY:
          return 'cr:security';
        case management.ReportingType.DEVICE:
          return 'cr:computer';
        case management.ReportingType.EXTENSIONS:
          return 'cr:extension';
        case management.ReportingType.USER:
          return 'management:account-circle';
        case management.ReportingType.USER_ACTIVITY:
          return 'management:public';
        default:
          return 'cr:security';
      }
    },

    /**
     * Handles the 'search-changed' event fired from the toolbar.
     * Redirects to the settings page initialized the the current
     * search query.
     * @param {!CustomEvent<string>} e
     * @private
     */
    onSearchChanged_: function(e) {
      const query = e.detail;
      window.location.href =
          `chrome://settings?search=${encodeURIComponent(query)}`;
    },

    /** @private */
    onTapBack_() {
      if (history.length > 1) {
        history.back();
      } else {
        window.location.href = 'chrome://settings/help';
      }
    },

    /** @private */
    updateManagedFields_() {
      this.browserProxy_.getContextualManagedData().then(data => {
        this.managed_ = data.managed;
        this.extensionReportingSubtitle_ = data.extensionReportingTitle;
        this.subtitle_ = data.pageSubtitle;
        // <if expr="chromeos">
        this.customerLogo_ = data.customerLogo;
        this.managementOverview_ = data.overview;
        // </if>
        // <if expr="not chromeos">
        this.managementNoticeHtml_ = data.browserManagementNotice;
        // </if>
      });
    },
  });

  return {
    BrowserReportingData: BrowserReportingData,
  };
});
