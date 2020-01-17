// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-downloads-page' is the settings page containing downloads
 * settings.
 */
Polymer({
  is: 'settings-downloads-page',

  behaviors: [WebUIListenerBehavior, PrefsBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    autoOpenDownloads_: {
      type: Boolean,
      value: false,
    },

    // <if expr="chromeos">
    /**
     * The download location string that is suitable to display in the UI.
     */
    downloadLocation_: String,
    // </if>
  },

  // <if expr="chromeos">
  observers: [
    'handleDownloadLocationChanged_(prefs.download.default_directory.value)'
  ],
  // </if>

  /** @private {?settings.DownloadsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.DownloadsBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.addWebUIListener('auto-open-downloads-changed', autoOpen => {
      this.autoOpenDownloads_ = autoOpen;
    });

    this.browserProxy_.initializeDownloads();
  },

  /** @private */
  selectDownloadLocation_() {
    listenOnce(this, 'transitionend', () => {
      this.browserProxy_.selectDownloadLocation();
    });
  },

  // <if expr="chromeos">
  /**
   * @private
   */
  handleDownloadLocationChanged_() {
    this.browserProxy_
        .getDownloadLocationText(/** @type {string} */ (
            this.getPref('download.default_directory').value))
        .then(text => {
          this.downloadLocation_ = text;
        });
  },
  // </if>

  /** @private */
  onClearAutoOpenFileTypesTap_() {
    this.browserProxy_.resetAutoOpenFileTypes();
  },
});
