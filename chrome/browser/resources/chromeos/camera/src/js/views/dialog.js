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
 * Creates the Dialog view controller.
 */
cca.views.Dialog = class extends cca.views.View {
  /**
   * @param {cca.views.ViewName} name View name of the dialog.
   */
  constructor(name) {
    super(name, true);

    /**
     * @type {!HTMLButtonElement}
     * @private
     */
    this.positiveButton_ = cca.assertInstanceof(
        this.root.querySelector('.dialog-positive-button'), HTMLButtonElement);

    /**
     * @type {!HTMLButtonElement}
     * @private
     */
    this.negativeButton_ = cca.assertInstanceof(
        this.root.querySelector('.dialog-negative-button'), HTMLButtonElement);

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.messageHolder_ = cca.assertInstanceof(
        this.root.querySelector('.dialog-msg-holder'), HTMLElement);

    this.positiveButton_.addEventListener('click', () => this.leave(true));
    if (this.negativeButton_) {
      this.negativeButton_.addEventListener('click', () => this.leave());
    }
  }

  /**
   * @override
   */
  entering({message, cancellable = false} = {}) {
    message = cca.assertString(message);
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
