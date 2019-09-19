// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for mojo.
 */
cca.mojo = cca.mojo || {};

/**
 * Communicates with Chrome.
 */
cca.mojo.ChromeHelper = class {
  /**
   * @public
   */
  constructor() {
    /**
     * An interface remote that is used to communicate with Chrome.
     * @type {cros.mojom.CameraAppHelperRemote}
     */
    this.remote_ = cros.mojom.CameraAppHelper.getRemote();
  }

  /**
   * Checks if the device is under tablet mode currently.
   * @return {!Promise<boolean>}
   */
  async isTabletMode() {
    return await this.remote_.isTabletMode().then(({isTabletMode}) => {
      return isTabletMode;
    });
  }

  /**
   * Creates a new instance of ChromeHelper if it is not set. Returns the
   *     exist instance.
   * @return {!cca.mojo.ChromeHelper} The singleton instance.
   */
  static getInstance() {
    if (this.instance === null) {
      this.instance = new cca.mojo.ChromeHelper();
    }
    return this.instance;
  }
};

/**
 * The singleton instance of ChromeHelper. Initialized by the first
 * invocation of getInstance().
 * @type {?cca.mojo.ChromeHelper}
 */
cca.mojo.ChromeHelper.instance = null;
