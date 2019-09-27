// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

Polymer({
  is: 'crostini-installer-app',

  _template: html`{__html_template__}`,

  /** @override */
  attached: function() {
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;
    // TODO(lxj)
  },

  /** @override */
  detached: function() {
    // TODO(lxj)
  },

  /** @private */
  onInstallButtonClick_: function() {
    // TODO(lxj)
  },
});
