// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-progress' component shows a progress indicator for
 * a Nearby Share transfer to a remote device. It shows device icon and name,
 * and a circular progress bar that only supports an indeterminate status for
 * now.
 */

import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'nearby-progress',

  _template: html`{__html_template__}`,

  properties: {
    /** The device name to show below the progress spinner. */
    deviceName: {
      type: String,
      value: '',
    },
  },
});
