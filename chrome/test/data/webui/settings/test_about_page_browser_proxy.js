// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// #import {isMac} from 'chrome://resources/js/cr.m.js';
// #import {UpdateStatus} from 'chrome://settings/settings.js';

/** @implements {settings.AboutPageBrowserProxy} */
/* #export */ class TestAboutPageBrowserProxy extends TestBrowserProxy {
  constructor() {
    const methodNames = [
      'pageReady',
      'refreshUpdateStatus',
      'openHelpPage',
      'openFeedbackDialog',
    ];

    if (cr.isMac) {
      methodNames.push('promoteUpdater');
    }

    super(methodNames);

    /** @private {!UpdateStatus} */
    this.updateStatus_ = UpdateStatus.UPDATED;
  }

  /** @param {!UpdateStatus} updateStatus */
  setUpdateStatus(updateStatus) {
    this.updateStatus_ = updateStatus;
  }

  sendStatusNoInternet() {
    cr.webUIListenerCallback('update-status-changed', {
      progress: 0,
      status: UpdateStatus.FAILED,
      message: 'offline',
      connectionTypes: 'no internet',
    });
  }

  /** @override */
  pageReady() {
    this.methodCalled('pageReady');
  }

  /** @override */
  refreshUpdateStatus() {
    cr.webUIListenerCallback('update-status-changed', {
      progress: 1,
      status: this.updateStatus_,
    });
    this.methodCalled('refreshUpdateStatus');
  }

  /** @override */
  openFeedbackDialog() {
    this.methodCalled('openFeedbackDialog');
  }

  /** @override */
  openHelpPage() {
    this.methodCalled('openHelpPage');
  }
}

if (cr.isMac) {
  /** @override */
  TestAboutPageBrowserProxy.prototype.promoteUpdater = function() {
    this.methodCalled('promoteUpdater');
  };
}

