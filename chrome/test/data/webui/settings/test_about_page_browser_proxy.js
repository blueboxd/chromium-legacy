// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.AboutPageBrowserProxy} */
class TestAboutPageBrowserProxy extends TestBrowserProxy {
  constructor() {
    const methodNames = [
      'pageReady',
      'refreshUpdateStatus',
      'openHelpPage',
      'openFeedbackDialog',
    ];

    if (cr.isChromeOS) {
      methodNames.push(
          'getChannelInfo', 'getVersionInfo', 'getRegulatoryInfo',
          'checkInternetConnection', 'getEnabledReleaseNotes',
          'getHasEndOfLife', 'openOsHelpPage', 'refreshTPMFirmwareUpdateStatus',
          'setChannel');
    }

    if (cr.isMac) {
      methodNames.push('promoteUpdater');
    }

    super(methodNames);

    /** @private {!UpdateStatus} */
    this.updateStatus_ = UpdateStatus.UPDATED;

    if (cr.isChromeOS) {
      /** @private {!VersionInfo} */
      this.versionInfo_ = {
        arcVersion: '',
        osFirmware: '',
        osVersion: '',
      };

      /** @private {!ChannelInfo} */
      this.channelInfo_ = {
        currentChannel: BrowserChannel.BETA,
        targetChannel: BrowserChannel.BETA,
        canChangeChannel: true,
      };

      /** @private {?RegulatoryInfo} */
      this.regulatoryInfo_ = null;

      /** @private {!TPMFirmwareUpdateStatus} */
      this.tpmFirmwareUpdateStatus_ = {
        updateAvailable: false,
      };

      /** @private {boolean|Promise} */
      this.hasEndOfLife_ = false;
    }
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

if (cr.isChromeOS) {
  /** @param {!VersionInfo} */
  TestAboutPageBrowserProxy.prototype.setVersionInfo = function(versionInfo) {
    this.versionInfo_ = versionInfo;
  };

  /** @param {boolean} canChangeChannel */
  TestAboutPageBrowserProxy.prototype.setCanChangeChannel = function(
      canChangeChannel) {
    this.channelInfo_.canChangeChannel = canChangeChannel;
  };

  /**
   * @param {!BrowserChannel} current
   * @param {!BrowserChannel} target
   */
  TestAboutPageBrowserProxy.prototype.setChannels = function(current, target) {
    this.channelInfo_.currentChannel = current;
    this.channelInfo_.targetChannel = target;
  };

  /** @param {?RegulatoryInfo} regulatoryInfo */
  TestAboutPageBrowserProxy.prototype.setRegulatoryInfo = function(
      regulatoryInfo) {
    this.regulatoryInfo_ = regulatoryInfo;
  };

  /** @param {boolean|Promise} hasEndOfLife */
  TestAboutPageBrowserProxy.prototype.setHasEndOfLife = function(hasEndOfLife) {
    this.hasEndOfLife_ = hasEndOfLife;
  };

  /** @param {boolean|Promise} hasReleaseNotes */
  TestAboutPageBrowserProxy.prototype.setReleaseNotes = function(
      hasEnabledReleaseNotes) {
    this.hasReleaseNotes_ = hasEnabledReleaseNotes;
  };

  /** @param {boolean|Promise} hasInternetConnection */
  TestAboutPageBrowserProxy.prototype.setInternetConnection = function(
      hasInternetConnection) {
    this.hasInternetConnection_ = hasInternetConnection;
  };

  /** @override */
  TestAboutPageBrowserProxy.prototype.getChannelInfo = function() {
    this.methodCalled('getChannelInfo');
    return Promise.resolve(this.channelInfo_);
  };

  /** @override */
  TestAboutPageBrowserProxy.prototype.getVersionInfo = function() {
    this.methodCalled('getVersionInfo');
    return Promise.resolve(this.versionInfo_);
  };

  /** @override */
  TestAboutPageBrowserProxy.prototype.getEnabledReleaseNotes = function() {
    this.methodCalled('getEnabledReleaseNotes');
    return Promise.resolve(this.hasReleaseNotes_);
  };

  /** @override */
  TestAboutPageBrowserProxy.prototype.checkInternetConnection = function() {
    this.methodCalled('checkInternetConnection');
    return Promise.resolve(this.hasInternetConnection_);
  };

  /** @override */
  TestAboutPageBrowserProxy.prototype.getRegulatoryInfo = function() {
    this.methodCalled('getRegulatoryInfo');
    return Promise.resolve(this.regulatoryInfo_);
  };

  /** @override */
  TestAboutPageBrowserProxy.prototype.getHasEndOfLife = function() {
    this.methodCalled('getHasEndOfLife');
    return Promise.resolve(this.hasEndOfLife_);
  };

  /** @override */
  TestAboutPageBrowserProxy.prototype.setChannel = function(
      channel, isPowerwashAllowed) {
    this.methodCalled('setChannel', [channel, isPowerwashAllowed]);
  };

  /** @param {!TPMFirmwareUpdateStatus} status */
  TestAboutPageBrowserProxy.prototype.setTPMFirmwareUpdateStatus = function(
      status) {
    this.tpmFirmwareUpdateStatus_ = status;
  };

  /** @override */
  TestAboutPageBrowserProxy.prototype.refreshTPMFirmwareUpdateStatus =
      function() {
    this.methodCalled('refreshTPMFirmwareUpdateStatus');
    cr.webUIListenerCallback(
        'tpm-firmware-update-status-changed', this.tpmFirmwareUpdateStatus_);
  };

  /** @override */
  TestAboutPageBrowserProxy.prototype.openOsHelpPage = function() {
    this.methodCalled('openOsHelpPage');
  };
}
