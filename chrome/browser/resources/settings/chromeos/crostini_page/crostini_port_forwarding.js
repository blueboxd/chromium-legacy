// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-port-forwarding' is the settings port forwarding subpage for
 * Crostini.
 */

const TCP_INDEX = 0;
const UDP_INDEX = 1;

Polymer({
  is: 'settings-crostini-port-forwarding',

  behaviors: [PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    showAddPortDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * The forwarded ports for display in the UI.
     * @private {!Array<!CrostiniPortSetting>}
     */
    ports_: {type: Array, value: []}
  },

  observers:
      ['onCrostiniPortsChanged_(prefs.crostini.port_forwarding.ports.value)'],

  /**
   * @param {!Array<!CrostiniPortSetting>} ports
   * @private
   */
  onCrostiniPortsChanged_: function(ports) {
    this.splice('ports_', 0, this.ports_.length);
    for (const port of ports) {
      port.is_active_pref = {
        key: '',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: port.active,
      };
      this.push('ports_', port);
    }
  },

  onAddPortClick_: function(event) {
    this.showAddPortDialog_ = true;
  },

  onAddPortDialogClose_: function(event) {
    this.showAddPortDialog_ = false;
  },
});
