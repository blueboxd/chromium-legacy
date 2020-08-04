// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {ContactUpdate} from './types.js';

/**
 * JavaScript hooks into the native WebUI handler to pass Contacts to the
 * Contacts tab.
 */
export class NearbyContactBrowserProxy {
  /**
   * Initializes web contents in the WebUI handler.
   */
  initialize() {
    chrome.send('initializeContacts');
  }

  /**
   * Downloads the user's contact list from the Nearby Share server. We can
   * force a download by passing |onlyDownloadIfContactsChanged| as false,
   * or we can choose to only download contacts if they have changed since the
   * last RPC call by passing in true.
   * @param {boolean} onlyDownloadIfContactsChanged
   */
  downloadContacts(onlyDownloadIfContactsChanged) {
    chrome.send('downloadContacts', [onlyDownloadIfContactsChanged]);
  }
}

addSingletonGetter(NearbyContactBrowserProxy);
