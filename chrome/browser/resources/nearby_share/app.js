// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-share' component is the entry point for the Nearby
 * Share flow. It is used as a standalone dialog via chrome://nearby and as part
 * of the ChromeOS share sheet.
 */

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.m.js';
import './nearby_discovery_page.js';
import './strings.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {string} */
const Page = {
  DISCOVERY: 'discovery',
};

Polymer({
  is: 'nearby-share-app',

  _template: html`{__html_template__}`,

  properties: {
    /** Mirroring the enum so that it can be used from HTML bindings. */
    Page: {
      type: Object,
      value: Page,
    },
  },
});
