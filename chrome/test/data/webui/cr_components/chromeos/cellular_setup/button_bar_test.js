// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/button_bar.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertFalse, assertTrue} from '../../../chai_assert.js';
// clang-format on

suite('CellularSetupButtonBarTest', function() {
  let buttonBar;
  setup(function() {
    buttonBar = document.createElement('button-bar');
    document.body.appendChild(buttonBar);
    Polymer.dom.flush();
  });

  test('Base test', function() {
    const title = buttonBar.$$('cr-button');
    assertTrue(!!title);
  });
});
