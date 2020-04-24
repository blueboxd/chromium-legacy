// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cloud-printers' is a component for showing Google
 * Cloud Printer settings subpage (chrome://settings/cloudPrinters).
 */
// TODO(xdai): Rename it to 'settings-cloud-printers-page'.
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_toggle_button.m.js';
import '../settings_shared_css.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'settings-cloud-printers',

  _template: html`{__html_template__}`,

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },
  },
});
