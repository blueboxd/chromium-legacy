// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-form-warning',
  properties: {
    strings: Object,
  },

  /** @private {PromiseResolver} */
  resolver_: null,

  show: function() {
    this.resolver_ = new PromiseResolver();
    /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
    return this.resolver_.promise;
  },

  onCancel: function() {
    this.resolver_.reject();
    this.$.dialog.cancel();
  },

  onAction: function() {
    this.resolver_.resolve();
    this.$.dialog.close();
  },
});
