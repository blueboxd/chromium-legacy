// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class provides a "bridge" for communicating between the javascript and
 * the browser.
 */
const BrowserBridge = (function() {
  'use strict';

  /**
   * @constructor
   */
  function BrowserBridge() {
    assertFirstConstructorCall(BrowserBridge);

    // List of observers for various bits of browser state.
    this.hstsObservers_ = [];
    this.expectCTObservers_ = [];
    this.setNetworkDebugModeObservers_ = [];
  }

  cr.addSingletonGetter(BrowserBridge);

  BrowserBridge.prototype = {

    //--------------------------------------------------------------------------
    // Messages sent to the browser
    //--------------------------------------------------------------------------

    /**
     * Wraps |chrome.send|.
     * TODO(mattm): remove this and switch things to use chrome.send directly.
     */
    send(value1, value2) {
      if (arguments.length == 1) {
        chrome.send(value1);
      } else if (arguments.length == 2) {
        chrome.send(value1, value2);
      } else {
        throw 'Unsupported number of arguments.';
      }
    },

    sendReloadProxySettings() {
      this.send('reloadProxySettings');
    },

    sendClearBadProxies() {
      this.send('clearBadProxies');
    },

    sendClearHostResolverCache() {
      this.send('clearHostResolverCache');
    },

    sendHSTSQuery(domain) {
      this.send('hstsQuery', [domain]);
    },

    sendHSTSAdd(domain, sts_include_subdomains) {
      this.send('hstsAdd', [domain, sts_include_subdomains]);
    },

    sendDomainSecurityPolicyDelete(domain) {
      this.send('domainSecurityPolicyDelete', [domain]);
    },

    sendExpectCTQuery(domain) {
      this.send('expectCTQuery', [domain]);
    },

    sendExpectCTAdd(domain, report_uri, enforce) {
      this.send('expectCTAdd', [domain, report_uri, enforce]);
    },

    sendExpectCTTestReport(report_uri) {
      this.send('expectCTTestReport', [report_uri]);
    },

    sendCloseIdleSockets() {
      this.send('closeIdleSockets');
    },

    sendFlushSocketPools() {
      this.send('flushSocketPools');
    },

    setNetworkDebugMode(subsystem) {
      this.send('setNetworkDebugMode', [subsystem]);
    },

    //--------------------------------------------------------------------------
    // Messages received from the browser.
    //--------------------------------------------------------------------------

    receive(command, params) {
      this[command](params);
    },

    receivedHSTSResult(info) {
      for (let i = 0; i < this.hstsObservers_.length; i++) {
        this.hstsObservers_[i].onHSTSQueryResult(info);
      }
    },

    receivedExpectCTResult(info) {
      for (let i = 0; i < this.expectCTObservers_.length; i++) {
        this.expectCTObservers_[i].onExpectCTQueryResult(info);
      }
    },

    receivedExpectCTTestReportResult(result) {
      for (let i = 0; i < this.expectCTObservers_.length; i++) {
        this.expectCTObservers_[i].onExpectCTTestReportResult(result);
      }
    },

    receivedSetNetworkDebugMode(status) {
      for (let i = 0; i < this.setNetworkDebugModeObservers_.length; i++) {
        this.setNetworkDebugModeObservers_[i].onSetNetworkDebugMode(status);
      }
    },

    //--------------------------------------------------------------------------

    /**
     * Adds a listener for the results of HSTS (HTTPS Strict Transport Security)
     * queries. The observer will be called back with:
     *
     *   observer.onHSTSQueryResult(result);
     */
    addHSTSObserver(observer) {
      this.hstsObservers_.push(observer);
    },

    /**
     * Adds a listener for the results of Expect-CT queries. The observer will
     * be called back with:
     *
     *   observer.onExpectCTQueryResult(result);
     */
    addExpectCTObserver(observer) {
      this.expectCTObservers_.push(observer);
    },

    /**
     * Adds a listener for network debugging mode status. The observer
     * will be called back with:
     *
     *   observer.onSetNetworkDebugMode(status);
     */
    addSetNetworkDebugModeObserver(observer) {
      this.setNetworkDebugModeObservers_.push(observer);
    },
  };

  return BrowserBridge;
})();
