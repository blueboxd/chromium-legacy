// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('device_emulator', {
  audioSettings: null,
  batterySettings: null,
  bluetoothSettings: null,
});

Polymer({
  is: 'device-emulator-pages',

  properties: {
    selectedPage: {
      type: Number,
      value: 0,
      observer: 'onSelectedPageChange_',
    },
  },

  /** @override */
  ready: function() {
    for (const page of this.$$('iron-pages').children)
      device_emulator[page.id] = page;

    chrome.send('initializeDeviceEmulator');
  },

  /** @private */
  onMenuButtonClick_: function() {
    this.$.drawer.toggle();
  },

  /** @private */
  onSelectedPageChange_: function() {
    this.$.drawer.close();
  },
});
