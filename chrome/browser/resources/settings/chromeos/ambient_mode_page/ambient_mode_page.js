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

    // TODO(b/160632748): Dynamically generate topic source of Google Photos.
    /** @private {!Array<!AmbientModeTopicSource>} */
    topicSources_: {
      type: Array,
      value: [
        AmbientModeTopicSource.GOOGLE_PHOTOS, AmbientModeTopicSource.ART_GALLERY
      ],
    },

    /** @private {!AmbientModeTopicSource} */
    selectedTopicSource_: {
      type: AmbientModeTopicSource,
      value: AmbientModeTopicSource.UNKNOWN,
    },
  },

  listeners: {
    'selected-topic-source-changed': 'onSelectedTopicSourceChanged_',
    'show-albums': 'onShowAlbums_',
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

  /**
   * @param {number} topicSource
   * @return {boolean}
   * @private
   */
  isValidTopicSource_(topicSource) {
    return topicSource !== AmbientModeTopicSource.UNKNOWN;
  },

  /**
   * @param {!CustomEvent<{item: !AmbientModeTopicSource}>} event
   * @private
   */
  onSelectedTopicSourceChanged_(event) {
    this.browserProxy_.setSelectedTopicSource(
        /** @type {!AmbientModeTopicSource} */ (event.detail));
  },

  /**
   * Open ambientMode/photos subpage.
   * @param {!CustomEvent<{item: !AmbientModeTopicSource}>} event
   * @private
   */
  onShowAlbums_(event) {
    const params = new URLSearchParams();
    params.append('topicSource', JSON.stringify(event.detail));
    settings.Router.getInstance().navigateTo(
        settings.routes.AMBIENT_MODE_PHOTOS, params);
  }
});
