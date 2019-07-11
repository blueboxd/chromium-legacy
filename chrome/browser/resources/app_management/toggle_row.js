// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-toggle-row',

  properties: {
    /**
     * @type {string}
     * @private
     */
    icon_: String,
    /**
     * @type {string}
     * @private
     */
    label_: String,
    /**
     * @type {boolean}
     * @private
     */
    managed_: {type: Boolean, value: false, reflectToAttribute: true},
    /**
     * @type {string}
     * @private
     */
    policyLabel_: String,
    /**
     * @type {boolean}
     * @private
     */
    value_: {type: Boolean, value: false, reflectToAttribute: true},
  },
});
