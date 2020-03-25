// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-ambient-mode-page' is the settings page containing
 * ambient mode settings.
 */
Polymer({
  is: 'settings-ambient-mode-page',

  behaviors: [I18nBehavior, PrefsBehavior],

  properties: {
    prefs: Object,

    /**
     * Enum values for the 'settings.ambient_mode.topic_source' preference.
     * Values need to stay in sync with the |ash::ambient::prefs::TopicSource|.
     * @private {!Object<string, number>}
     */
    topicSource_: {
      type: Object,
      value: {
        GOOGLE_PHOTOS: 0,
        ART_GALLERY: 1,
      },
      readOnly: true,
    },

    /** @private */
    isAmbientModeEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isAmbientModeEnabled');
      },
      readOnly: true,
    },
  },

  /** @private {?settings.AmbientModeBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.AmbientModeBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.browserProxy_.onAmbientModePageReady();
  },

  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAmbientModeOnOffLabel_(toggleValue) {
    return this.i18n(toggleValue ? 'ambientModeOn' : 'ambientModeOff');
  },
});
