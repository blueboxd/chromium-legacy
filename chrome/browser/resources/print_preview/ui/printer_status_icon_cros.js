// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './icons.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Enumeration giving a local Chrome OS printer 3 different state possibilities
 * depending on its current status.
 * @enum {number}
 */
export const PrinterState = {
  GOOD: 0,
  ERROR: 1,
  UNKNOWN: 2,
};

/**
 * Enumeration used to choose styling based on whether this icon is located in
 * the destination display or the destination dropdown.
 * @enum {number}
 */
export const IconLocation = {
  DISPLAY: 0,
  DROPDOWN: 1,
};

Polymer({
  is: 'printer-status-icon-cros',

  properties: {
    /** Determines color of the background badge. */
    background: String,

    /**
     * State of the associated printer. Determines color of the status badge.
     * @type {!PrinterState}
     */
    printerState: {
      type: Number,
      reflectToAttribute: true,
    },

    /**
     * Location of this icon. Determines color of the background badge.
     * @type {!IconLocation}
     */
    iconLocation: {
      type: Number,
      reflectToAttribute: true,
    },
  },

  _template: html`{__html_template__}`,

});
