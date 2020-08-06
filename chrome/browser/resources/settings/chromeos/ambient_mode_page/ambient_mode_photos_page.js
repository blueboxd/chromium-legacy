// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-ambient-mode-photos-page' is the settings page to
 * select personal albums in Google Photos or categories in Art gallary.
 */
Polymer({
  is: 'settings-ambient-mode-photos-page',

  behaviors:
      [I18nBehavior, settings.RouteObserverBehavior, WebUIListenerBehavior],

  properties: {
    /** @private {!AmbientModeTopicSource} */
    topicSource_: {
      type: AmbientModeTopicSource,
      value() {
        return AmbientModeTopicSource.UNKNOWN;
      },
    },

    /** @private {Array<!AmbientModeAlbum>} */
    albums_: Array,
  },

  /** @private {?settings.AmbientModeBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.AmbientModeBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.addWebUIListener('albums-changed', this.onAlbumsChanged_.bind(this));
  },

  /**
   * RouteObserverBehavior
   * @param {!settings.Route} currentRoute
   * @protected
   */
  currentRouteChanged(currentRoute) {
    if (currentRoute !== settings.routes.AMBIENT_MODE_PHOTOS) {
      return;
    }

    const topicSourceParam =
        settings.Router.getInstance().getQueryParameters().get('topicSource');
    const topicSourceInt = parseInt(topicSourceParam, 10);

    if (isNaN(topicSourceInt)) {
      assertNotReached();
      return;
    }

    this.topicSource_ = /** @type {!AmbientModeTopicSource} */ (topicSourceInt);
    if (this.topicSource_ === AmbientModeTopicSource.GOOGLE_PHOTOS) {
      this.parentNode.pageTitle =
          this.i18n('ambientModeTopicSourceGooglePhotos');
    } else if (this.topicSource_ === AmbientModeTopicSource.ART_GALLERY) {
      this.parentNode.pageTitle = this.i18n('ambientModeTopicSourceArtGallery');
    } else {
      assertNotReached();
      return;
    }

    this.albums_ = [];
    this.browserProxy_.requestAlbums(this.topicSource_);
  },

  /**
   * @param {!AmbientModeSettings} settings
   * @private
   */
  onAlbumsChanged_(settings) {
    // This page has been reused by other topic source since the last time
    // requesting the albums. Do not update on this stale event.
    if (settings.topicSource !== this.topicSource_) {
      return;
    }
    this.albums_ = settings.albums;
  },

  /**
   * @param {number} topicSource
   * @return {string}
   * @private
   */
  getTitleInnerHtml_(topicSource) {
    if (topicSource === AmbientModeTopicSource.GOOGLE_PHOTOS) {
      return this.i18nAdvanced('ambientModeAlbumsSubpageGooglePhotosTitle');
    } else {
      return this.i18n('ambientModeTopicSourceArtGalleryDescription');
    }
  },

  /** @private */
  onCheckboxChange_() {
    const checkboxes = this.$$('#albums').querySelectorAll('cr-checkbox');
    const albums = [];
    checkboxes.forEach((checkbox) => {
      if (checkbox.checked && !checkbox.hidden) {
        albums.push({
          albumId: checkbox.dataset.id,
          checked: true,
          title: checkbox.label
        });
      }
    });
    this.browserProxy_.setSelectedAlbums(
        {topicSource: this.topicSource_, albums: albums});
  }

});
