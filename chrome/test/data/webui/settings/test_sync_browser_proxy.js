// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {PageStatus} from 'chrome://settings/settings.js';
// #import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// #import {isChromeOS} from 'chrome://resources/js/cr.m.js';

/** @implements {settings.SyncBrowserProxy} */
/* #export */ class TestSyncBrowserProxy extends TestBrowserProxy {
  constructor() {
    const methodNames = [
      'didNavigateAwayFromSyncPage',
      'didNavigateToSyncPage',
      'getPromoImpressionCount',
      'getStoredAccounts',
      'getSyncStatus',
      'incrementPromoImpressionCount',
      'setSyncDatatypes',
      'setSyncEncryption',
      'signOut',
      'pauseSync',
      'sendSyncPrefsChanged',
      'startSignIn',
      'startSyncingWithEmail',
    ];

    if (cr.isChromeOS) {
      methodNames.push('turnOnSync', 'turnOffSync');
    }

    super(methodNames);

    /** @private {number} */
    this.impressionCount_ = 0;

    /** @type {!settings.PageStatus} */
    this.encryptionResponse = settings.PageStatus.CONFIGURE;
  }

  /** @override */
  getSyncStatus() {
    this.methodCalled('getSyncStatus');
    return Promise.resolve({signedIn: true, signedInUsername: 'fakeUsername'});
  }

  /** @override */
  getStoredAccounts() {
    this.methodCalled('getStoredAccounts');
    return Promise.resolve([]);
  }

  /** @override */
  signOut(deleteProfile) {
    this.methodCalled('signOut', deleteProfile);
  }

  /** @override */
  pauseSync() {
    this.methodCalled('pauseSync');
  }

  /** @override */
  startSignIn() {
    this.methodCalled('startSignIn');
  }

  /** @override */
  startSyncingWithEmail(email, isDefaultPromoAccount) {
    this.methodCalled('startSyncingWithEmail', [email, isDefaultPromoAccount]);
  }

  setImpressionCount(count) {
    this.impressionCount_ = count;
  }

  /** @override */
  getPromoImpressionCount() {
    this.methodCalled('getPromoImpressionCount');
    return this.impressionCount_;
  }

  /** @override */
  incrementPromoImpressionCount() {
    this.methodCalled('incrementPromoImpressionCount');
  }

  /** @override */
  didNavigateToSyncPage() {
    this.methodCalled('didNavigateToSyncPage');
  }

  /** @override */
  didNavigateAwayFromSyncPage(abort) {
    this.methodCalled('didNavigateAwayFromSyncPage', abort);
  }

  /** @override */
  setSyncDatatypes(syncPrefs) {
    this.methodCalled('setSyncDatatypes', syncPrefs);
    return Promise.resolve(settings.PageStatus.CONFIGURE);
  }

  /** @override */
  setSyncEncryption(syncPrefs) {
    this.methodCalled('setSyncEncryption', syncPrefs);
    return Promise.resolve(this.encryptionResponse);
  }

  /** @override */
  sendSyncPrefsChanged() {
    this.methodCalled('sendSyncPrefsChanged');
  }
}

if (cr.isChromeOS) {
  /** @override */
  TestSyncBrowserProxy.prototype.turnOnSync = function() {
    this.methodCalled('turnOnSync');
  };

  /** @override */
  TestSyncBrowserProxy.prototype.turnOffSync = function() {
    this.methodCalled('turnOffSync');
  };
}
