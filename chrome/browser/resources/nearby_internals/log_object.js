// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Severity} from './types.js';

Polymer({
  is: 'log-object',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * Underlying LogMessage data for this item. Contains read-only fields
     * from the NearbyShare backend, as well as fields computed by logging tab.
     * Type: {!LogMessage}
     */
    item: {
      type: Object,
      observer: 'itemChanged_',
    },
  },

  /**
   * Sets the log message style based on severity level.
   * @private
   */
  itemChanged_() {
    switch (this.item.severity) {
      case Severity.WARNING:
        this.$['item'].className = 'warning-log';
        break;
      case Severity.ERROR:
        this.$['item'].className = 'error-log';
        break;
      case Severity.VERBOSE:
        this.$['item'].className = 'verbose-log';
        break;
      default:
        this.$['item'].className = 'default-log';
        break;
    }
  },
});
