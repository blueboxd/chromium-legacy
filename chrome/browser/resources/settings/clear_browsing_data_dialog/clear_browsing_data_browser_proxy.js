// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Clear browsing data" dialog
 * to interact with the browser.
 */

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * An InstalledApp represents a domain with data that the user might want
 * to protect from being deleted.
 *
 * @typedef {{
 *   registerableDomain: string,
 *   reasonBitfield: number,
 *   exampleOrigin: string,
 *   isChecked: boolean,
 *   storageSize: number,
 *   hasNotifications: boolean,
 *   appName: string
 * }}
 */
export let InstalledApp;

/** @interface */
export class ClearBrowsingDataBrowserProxy {
  /**
   * @param {!Array<string>} dataTypes
   * @param {number} timePeriod
   * @param {Array<InstalledApp>} installedApps
   * @return {!Promise<boolean>}
   *     A promise resolved when data clearing has completed. The boolean
   *     indicates whether an additional dialog should be shown, informing the
   *     user about other forms of browsing history.
   */
  clearBrowsingData(dataTypes, timePeriod, installedApps) {}

  /**
   * @param {number} timePeriod
   * @return {!Promise<!Array<!InstalledApp>>}
   *     A promise resolved after fetching all installed apps. The array
   *     will contain a list of origins for which there are installed apps.
   */
  getInstalledApps(timePeriod) {}

  /**
   * Kick off counter updates and return initial state.
   * @return {!Promise<void>} Signal when the setup is complete.
   */
  initialize() {}
}

/**
 * @implements {ClearBrowsingDataBrowserProxy}
 */
export class ClearBrowsingDataBrowserProxyImpl {
  /** @override */
  clearBrowsingData(dataTypes, timePeriod, installedApps) {
    return sendWithPromise(
        'clearBrowsingData', dataTypes, timePeriod, installedApps);
  }

  /** @override */
  getInstalledApps(timePeriod) {
    return sendWithPromise('getInstalledApps', timePeriod);
  }

  /** @override */
  initialize() {
    return sendWithPromise('initializeClearBrowsingData');
  }
}

addSingletonGetter(ClearBrowsingDataBrowserProxyImpl);
