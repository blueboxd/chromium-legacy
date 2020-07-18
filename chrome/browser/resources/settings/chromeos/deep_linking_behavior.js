// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Polymer behavior for scrolling/focusing/indicating
 * setting elements with deep links.
 */

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js'
// #import '../constants/setting.mojom-lite.js';

// #import {afterNextRender, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {Router} from '../router.m.js';
// clang-format on

const kDeepLinkSettingId = 'settingId';
const kDeepLinkFocusId = 'deep-link-focus-id';

/** @polymerBehavior */
/* #export */ const DeepLinkingBehavior = {
  properties: {
    /**
     * An object whose values are the kSetting mojom values which can be used to
     * define deep link IDs on HTML elems.
     * @type {!Object}
     */
    Setting: {
      type: Object,
      value: chromeos.settings.mojom.Setting,
    },
  },

  /**
   * Retrieves the settingId saved in the url's query parameter. Returns null if
   * deep linking is disabled or if no settingId is found.
   * @return {?chromeos.settings.mojom.Setting}
   */
  getDeepLinkSettingId() {
    if (!loadTimeData.getBoolean('isDeepLinkingEnabled')) {
      return null;
    }
    const settingIdStr = settings.Router.getInstance().getQueryParameters().get(
        kDeepLinkSettingId);
    const settingIdNum = Number(settingIdStr);
    if (isNaN(settingIdNum)) {
      return null;
    }
    return /** @type {!chromeos.settings.mojom.Setting} */ (settingIdNum);
  },

  /**
   * Focuses the deep linked element referred to by |settindId|.
   * @param {chromeos.settings.mojom.Setting} settingId
   */
  showDeepLink(settingId) {
    assert(loadTimeData.getBoolean('isDeepLinkingEnabled'));
    const elToFocus = this.$$(`[${kDeepLinkFocusId}~="${settingId}"]`);
    Polymer.RenderStatus.afterNextRender(this, () => {
      elToFocus.focus();
    });
  }
};
