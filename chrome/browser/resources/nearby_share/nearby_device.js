// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-device' component shows details of a remote device.
 */

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'nearby-device',

  _template: html`{__html_template__}`,

  properties: {
    /** The device name to show. */
    name: {
      type: String,
      value: '',
    },
  },
});
