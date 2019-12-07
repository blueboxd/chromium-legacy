// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for the background page.
 */
cca.bg = cca.bg || {};

/**
 * Operations supported by foreground window.
 * @interface
 */
cca.bg.ForegroundOps = class {
  /**
   * Suspend foreground window.
   * @return {!Promise}
   * @abstract
   */
  async suspend() {}

  /**
   * Resume foreground window.
   * @abstract
   */
  resume() {}
};

/**
 * Operations supported by background window.
 * @interface
 */
cca.bg.BackgroundOps = class {
  /**
   * Sets the implementation of ForegroundOps from foreground window.
   * @param {!cca.bg.ForegroundOps} ops
   */
  bindForegroundOps(ops) {}

  /**
   * Gets intent associate with cca.bg.Window object.
   * @return {?cca.intent.Intent}
   * @abstract
   */
  getIntent() {}

  /**
   * Called by foreground window when it's active.
   * @abstract
   */
  notifyActivation() {}

  /**
   * Called by foreground window when it's suspended.
   * @abstract
   */
  notifySuspension() {}
};
