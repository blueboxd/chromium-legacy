// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * This is the data structure sent back and forth between C++ and JS.
 * @typedef {{
 *   profilePath: string,
 *   localProfileName: string,
 *   gaiaName: string,
 *   avatarIcon: string,
 * }}
 */
export let ProfileState;

/** @interface */
export class ManageProfilesBrowserProxy {
  /**
   * Initializes profile picker main view.
   */
  initializeMainView() {}

  /**
   * Launches picked profile and closes the profile picker.
   * @param {string} profilePath
   */
  launchSelectedProfile(profilePath) {}
}

/** @implements {ManageProfilesBrowserProxy} */
export class ManageProfilesBrowserProxyImpl {
  /** @override */
  initializeMainView() {
    chrome.send('mainViewInitialize');
  }

  /** @override */
  launchSelectedProfile(profilePath) {
    chrome.send('launchSelectedProfile', [profilePath]);
  }
}

addSingletonGetter(ManageProfilesBrowserProxyImpl);
