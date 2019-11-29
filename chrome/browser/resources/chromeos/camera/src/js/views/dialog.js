// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * import {assertString} from '../chrome_util.js';
 */
var assertString = assertString || {};

/**
 * import {assertInstanceof, assertString, assertBoolean}
 * from '../chrome_util.js';
 */
var {assertInstanceof, assertString, assertBoolean} = {
  assertInstanceof,
  assertString,
  assertBoolean,
};

/**
 * Creates the Dialog view controller.
 */
cca.views.Dialog = class extends cca.views.View {
  /**
   * @param {string} viewId Root element id of dialog view.
   */
  constructor(viewId) {
    super(viewId, true);

    /**
     * @type {!HTMLButtonElement}
     * @private
     */
    this.positiveButton_ = assertInstanceof(
        document.querySelector(`${viewId} .dialog-positive-button`),
        HTMLButtonElement);

    /**
     * @type {!HTMLButtonElement}
     * @private
     */
    this.negativeButton_ = assertInstanceof(
        document.querySelector(`${viewId} .dialog-negative-button`),
        HTMLButtonElement);

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.messageHolder_ = assertInstanceof(
        document.querySelector(`${viewId} .dialog-msg-holder`), HTMLElement);

    this.positiveButton_.addEventListener('click', () => this.leave(true));
    if (this.negativeButton_) {
      this.negativeButton_.addEventListener('click', () => this.leave());
    }
  }

  /**
   * @override
   */
  entering({message, cancellable = false} = {}) {
    message = assertString(message);
    this.messageHolder_.textContent = message;
    if (this.negativeButton_) {
      this.negativeButton_.hidden = !cancellable;
    }
  }

  /**
   * @override
   */
  focus() {
    this.positiveButton_.focus();
  }
};
