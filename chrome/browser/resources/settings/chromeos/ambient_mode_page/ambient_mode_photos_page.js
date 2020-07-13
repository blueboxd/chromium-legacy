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

    /** @private {Array<Object>} */
    photosContainers_: Array,
  },

  /** @private {?settings.AmbientModeBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.AmbientModeBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'photos-containers-changed', (AmbientModeSettings) => {
          // This page has been reused by other topic source since the last time
          // requesting the containers. Do not update on this stale event.
          if (AmbientModeSettings.topicSource !== this.topicSource_) {
            return;
          }
          this.photosContainers_ = AmbientModeSettings.topicContainers;
        });
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

    this.photosContainers_ = [];
    this.browserProxy_.requestPhotosContainers(this.topicSource_);
  },

  /**
   * @param {number} topicSource
   * @return {string}
   * @private
   */
  getDescription_(topicSource) {
    // TODO(b/159766700, items 2&3): Finalize the strings and i18n.
    if (topicSource === AmbientModeTopicSource.GOOGLE_PHOTOS) {
      return 'A slideshow of selected memories will be created';
    } else {
      return 'Curated images and artwork';
    }
  },

  /** @private */
  onCheckboxChange_() {
    const checkboxes = this.$$('#containers').querySelectorAll('cr-checkbox');
    const containers = [];
    checkboxes.forEach((checkbox) => {
      if (checkbox.checked && !checkbox.hidden) {
        containers.push(checkbox.label);
      }
    });
    this.browserProxy_.setSelectedPhotosContainers(
        {topicSource: this.topicSource_, topicContainers: containers});
  }

});
