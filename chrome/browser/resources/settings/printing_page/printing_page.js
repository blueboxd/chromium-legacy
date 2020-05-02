// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cloud_printers.js';
import '../settings_page/settings_animated_pages.m.js';
import '../settings_page/settings_subpage.m.js';
import '../settings_shared_css.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Router} from '../router.m.js';

// <if expr="not chromeos">
import {PrintingBrowserProxyImpl} from './printing_browser_proxy.js';
// </if>

Polymer({
  is: 'settings-printing-page',

  _template: html`{__html_template__}`,

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    searchTerm: {
      type: String,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (routes.CLOUD_PRINTERS) {
          map.set(routes.CLOUD_PRINTERS.path, '#cloudPrinters');
        }
        return map;
      },
    },
  },

  // <if expr="not chromeos">
  onTapLocalPrinters_() {
    PrintingBrowserProxyImpl.getInstance().openSystemPrintDialog();
  },
  // </if>

  /** @private */
  onTapCloudPrinters_() {
    Router.getInstance().navigateTo(routes.CLOUD_PRINTERS);
  },
});
