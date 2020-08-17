// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/base_page.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('CrComponentsBasePageTest', function() {
  let basePage;
  setup(function() {
    basePage = document.createElement('base-page');
    document.body.appendChild(basePage);
    Polymer.dom.flush();
  });

  test('Base test', function() {
    const title = basePage.$$('#title');
    assertTrue(!!title);
  });
});
