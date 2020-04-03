// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getMetadataProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'print-management' is used as the main app to display print jobs.
 */
Polymer({
  is: 'print-management',

  _template: html`{__html_template__}`,

  /**
   * @private {
   *  ?chromeos.printing.printingManager.mojom.PrintingMetadataProviderInterface
   * }
   */
  mojoInterfaceProvider_: null,

  /** @override */
  created() {
    this.mojoInterfaceProvider_ = getMetadataProvider();
  },

  /** @override */
  ready() {
    // TODO(jimmyxgong): Remove this once the app has more capabilities.
    this.$$('#header').textContent = 'Print Management';
  },
});
