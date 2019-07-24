// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('management', function() {
  /**
   * @typedef {{
   *   name: string,
   *   permissions: !Array<string>
   * }}
   */
  let Extension;

  /** @enum {string} */
  const ReportingType = {
    SECURITY: 'security',
    DEVICE: 'device',
    USER: 'user',
    USER_ACTIVITY: 'user-activity',
    EXTENSIONS: 'extensions'
  };

  /**
   * @typedef {{
   *   messageId: string,
   *   reportingType: !management.ReportingType,
   * }}
   */
  let BrowserReportingResponse;

  /**
   * @typedef {{
   *   browserManagementNotice: string,
   *   extensionReportingTitle: string,
   *   pageSubtitle: string,
   *   managed: boolean,
   *   overview: string,
   *   customerLogo: string,
   * }}
   */
  let ManagedDataResponse;

  // <if expr="chromeos">
  /**
   * @enum {string} Look at ToJSDeviceReportingType usage in
   *    management_ui_handler.cc for more details.
   */
  const DeviceReportingType = {
    SUPERVISED_USER: 'supervised user',
    DEVICE_ACTIVITY: 'device activity',
    STATISTIC: 'device statistics',
    DEVICE: 'device',
    LOGS: 'logs',
    PRINT: 'print',
    CROSTINI: 'crostini'
  };


  /**
   * @typedef {{
   *   messageId: string,
   *   reportingType: !management.DeviceReportingType,
   * }}
   */
  let DeviceReportingResponse;
  // </if>

  /** @interface */
  class ManagementBrowserProxy {
    /** @return {!Promise<!Array<!management.Extension>>} */
    getExtensions() {}

    // <if expr="chromeos">
    /**
     * @return {!Promise<boolean>} Boolean describing trust root configured
     *     or not.
     */
    getLocalTrustRootsInfo() {}

    /**
     * @return {!Promise<!Array<management.DeviceReportingResponse>>} List of
     *     items to display in device reporting section.
     */
    getDeviceReportingInfo() {}
    // </if>

    /** @return {!Promise<!management.ManagedDataResponse>} */
    getContextualManagedData() {}

    /**
     * @return {!Promise<!Array<!management.BrowserReportingResponse>>} The list
     *     of browser reporting info messages.
     */
    initBrowserReportingInfo() {}
  }

  /** @implements {management.ManagementBrowserProxy} */
  class ManagementBrowserProxyImpl {
    /** @override */
    getExtensions() {
      return cr.sendWithPromise('getExtensions');
    }

    // <if expr="chromeos">
    /** @override */
    getLocalTrustRootsInfo() {
      return cr.sendWithPromise('getLocalTrustRootsInfo');
    }

    /** @override */
    getDeviceReportingInfo() {
      return cr.sendWithPromise('getDeviceReportingInfo');
    }
    // </if>

    /** @override */
    getContextualManagedData() {
      return cr.sendWithPromise('getContextualManagedData');
    }

    /** @override */
    initBrowserReportingInfo() {
      return cr.sendWithPromise('initBrowserReportingInfo');
    }
  }

  cr.addSingletonGetter(ManagementBrowserProxyImpl);

  return {
    BrowserReportingResponse: BrowserReportingResponse,
    // <if expr="chromeos">
    DeviceReportingResponse: DeviceReportingResponse,
    DeviceReportingType: DeviceReportingType,
    // </if>
    Extension: Extension,
    ManagedDataResponse: ManagedDataResponse,
    ManagementBrowserProxyImpl: ManagementBrowserProxyImpl,
    ManagementBrowserProxy: ManagementBrowserProxy,
    ReportingType: ReportingType,
  };
});
