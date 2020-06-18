// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './icons.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'printer-status-icon-cros',

  properties: {
    /** Determines color of the background badge. */
    background: String,

    /** Determines color of the status badge. */
    status: String,
  },

  _template: html`{__html_template__}`,

});
