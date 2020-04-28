// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-ambient-mode-page' is the settings page containing
 * ambient mode settings.
 */
Polymer({
  is: 'settings-ambient-mode-page',

  behaviors: [I18nBehavior, PrefsBehavior, WebUIListenerBehavior],

  properties: {
    prefs: Object,

    /**
     * Enum values for topicSourceRadioGroup.
     * Values need to stay in sync with the enum |ash::AmbientModeTopicSource|.
     * @private {!Object<string, number>}
     */
    topicSource_: {
      type: Object,
      value: {
        UNKNOWN: -1,
        GOOGLE_PHOTOS: 0,
        ART_GALLERY: 1,
      },
      readOnly: true,
    },

    /** @private */
    topicSourceSelected_: {
      type: String,
      value() {
        return this.topicSource_.UNKNOWN;
      },
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
    this.addWebUIListener('topic-source-changed', (topicSource) => {
      this.topicSourceSelected_ = topicSource;
    });

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

  /** @private */
  onTopicSourceSelectedChanged_() {
    return this.browserProxy_.onTopicSourceSelectedChanged(
        this.$$('#topicSourceRadioGroup').selected);
  },

  /**
   * @param {number} topicSource
   * @return {boolean}
   * @private
   */
  isValidTopicSource_(topicSource) {
    return this.topicSourceSelected_ !== this.topicSource_.UNKNOWN;
  },
});
