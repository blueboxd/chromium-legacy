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
    /** Preferences state. */
    prefs: Object,

    /**
     * Used to refer to the enum values in the HTML.
     * @private {!Object}
     */
    AmbientModeTopicSource: {
      type: Object,
      value: AmbientModeTopicSource,
    },

    /** @private {!AmbientModeTopicSource} */
    selectedTopicSource_: {
      type: AmbientModeTopicSource,
      value: AmbientModeTopicSource.UNKNOWN,
    },

    /** @private {!AmbientModeTopicSource} */
    previousTopicSource_: {
      type: AmbientModeTopicSource,
      value: AmbientModeTopicSource.UNKNOWN,
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
      this.selectedTopicSource_ = topicSource;
      this.previousTopicSource_ = topicSource;
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
  onTopicSourceClicked_() {
    const selectedTopicSource = this.$$('#topicSourceRadioGroup').selected;
    if (selectedTopicSource === '0') {
      this.selectedTopicSource_ = AmbientModeTopicSource.GOOGLE_PHOTOS;
    } else if (selectedTopicSource === '1') {
      this.selectedTopicSource_ = AmbientModeTopicSource.ART_GALLERY;
    }

    if (this.selectedTopicSource_ !== this.previousTopicSource_) {
      this.previousTopicSource_ = this.selectedTopicSource_;
      this.browserProxy_.setSelectedTopicSource(this.selectedTopicSource_);
    }

    const params = new URLSearchParams();
    params.append('topicSource', JSON.stringify(this.selectedTopicSource_));
    settings.Router.getInstance().navigateTo(
        settings.routes.AMBIENT_MODE_PHOTOS, params);
  },

  /**
   * @param {number} topicSource
   * @return {boolean}
   * @private
   */
  isValidTopicSource_(topicSource) {
    return topicSource !== AmbientModeTopicSource.UNKNOWN;
  },
});
