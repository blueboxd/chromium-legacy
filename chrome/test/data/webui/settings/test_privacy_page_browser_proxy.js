// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// #import {SecureDnsUiManagementMode, SecureDnsMode} from 'chrome://settings/settings.js';
// clang-format on

/** @implements {settings.PrivacyPageBrowserProxy} */
/* #export */ class TestPrivacyPageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getMetricsReporting',
      'recordSettingsPageHistogram',
      'setMetricsReportingEnabled',
      'showManageSSLCertificates',
      'setBlockAutoplayEnabled',
      'getSecureDnsResolverList',
      'getSecureDnsSetting',
      'validateCustomDnsEntry',
      'probeCustomDnsTemplate',
      'recordUserDropdownInteraction',
    ]);

    /** @type {!MetricsReporting} */
    this.metricsReporting = {
      enabled: true,
      managed: true,
    };

    /**
     * @type {!SecureDnsSetting}
     * @private
     */
    this.secureDnsSetting = {
      mode: settings.SecureDnsMode.SECURE,
      templates: [],
      managementMode: settings.SecureDnsUiManagementMode.NO_OVERRIDE,
    };

    /**
     * @type {!Array<!ResolverOption>}
     * @private
     */
    this.resolverList_;

    /**
     * @type {boolean}
     * @private
     */
    this.isEntryValid_;
  }

  /** @override */
  getMetricsReporting() {
    this.methodCalled('getMetricsReporting');
    return Promise.resolve(this.metricsReporting);
  }

  /** @override*/
  recordSettingsPageHistogram(value) {
    this.methodCalled('recordSettingsPageHistogram', value);
  }

  /** @override */
  setMetricsReportingEnabled(enabled) {
    this.methodCalled('setMetricsReportingEnabled', enabled);
  }

  /** @override */
  showManageSSLCertificates() {
    this.methodCalled('showManageSSLCertificates');
  }

  /** @override */
  setBlockAutoplayEnabled(enabled) {
    this.methodCalled('setBlockAutoplayEnabled', enabled);
  }

  /**
   * Sets the resolver list that will be returned when getSecureDnsResolverList
   * is called.
   * @param {!Array<!ResolverOption>} resolverList
   */
  setResolverList(resolverList) {
    this.resolverList_ = resolverList;
  }

  /** @override */
  getSecureDnsResolverList() {
    this.methodCalled('getSecureDnsResolverList');
    return Promise.resolve(this.resolverList_);
  }

  /** @override */
  getSecureDnsSetting() {
    this.methodCalled('getSecureDnsSetting');
    return Promise.resolve(this.secureDnsSetting);
  }

  /**
   * Sets the return value for the next validateCustomDnsEntry call.
   * @param {string} validEntry
   */
  setValidEntry(validEntry) {
    this.validEntry_ = validEntry;
  }

  /** @override */
  validateCustomDnsEntry(entry) {
    this.methodCalled('validateCustomDnsEntry', entry);
    return Promise.resolve(this.validEntry_);
  }

  /**
   * Sets the return value for the next probeCustomDnsTemplate call.
   * @param {boolean} success
   */
  setProbeSuccess(success) {
    this.probeSuccess_ = success;
  }

  /** @override */
  probeCustomDnsTemplate(template) {
    this.methodCalled('probeCustomDnsTemplate', template);
    return Promise.resolve(this.probeSuccess_);
  }

  /** @override */
  recordUserDropdownInteraction(oldSelection, newSelection) {
    this.methodCalled(
        'recordUserDropdownInteraction', [oldSelection, newSelection]);
  }
}
