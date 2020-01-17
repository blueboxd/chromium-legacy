// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * 'settings-personalization-page' is the settings page containing
 * personalization settings.
 */
Polymer({
  is: 'settings-personalization-page',

  properties: {
    /** @private */
    showWallpaperRow_: {type: Boolean, value: true},

    /** @private */
    isWallpaperPolicyControlled_: {type: Boolean, value: true},

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (settings.routes.CHANGE_PICTURE) {
          map.set(settings.routes.CHANGE_PICTURE.path, '#changePictureRow');
        }
        return map;
      }
    },
  },

  /** @private {?settings.WallpaperBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.WallpaperBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.browserProxy_.isWallpaperSettingVisible().then(
        isWallpaperSettingVisible => {
          this.showWallpaperRow_ = isWallpaperSettingVisible;
        });
    this.browserProxy_.isWallpaperPolicyControlled().then(
        isPolicyControlled => {
          this.isWallpaperPolicyControlled_ = isPolicyControlled;
        });
  },

  /**
   * @private
   */
  openWallpaperManager_() {
    this.browserProxy_.openWallpaperManager();
  },

  /** @private */
  navigateToChangePicture_() {
    settings.Router.getInstance().navigateTo(settings.routes.CHANGE_PICTURE);
  },
});
})();
