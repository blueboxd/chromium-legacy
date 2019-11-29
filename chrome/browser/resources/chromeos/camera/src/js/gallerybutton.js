// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * import {assertInstanceof} from './chrome_util.js';
 */
var assertInstanceof = assertInstanceof || {};

/**
 * Creates a controller for the gallery-button.
 * @implements {cca.models.Gallery.Observer}
 */
cca.GalleryButton = class {
  /**
   * @param {!cca.models.Gallery} model Model object.
   */
  constructor(model) {
    /**
     * @type {!cca.models.Gallery}
     * @private
     */
    this.model_ = model;

    /**
     * @type {?cca.models.Gallery.Picture}
     * @private
     */
    this.lastPicture_ = null;

    /**
     * @type {!HTMLButtonElement}
     * @private
     */
    this.button_ = assertInstanceof(
        document.querySelector('#gallery-enter'), HTMLButtonElement);

    this.button_.addEventListener('click', (event) => {
      // Check if the last picture still exists before opening it in the
      // gallery.
      // TODO(yuli): Remove this workaround for unable watching changed-files.
      this.model_.checkLastPicture().then((picture) => {
        if (picture) {
          this.openGallery_(picture);
        }
      });
    });
  }

  /**
   * Updates the button for the model changes.
   * @private
   */
  updateButton_() {
    this.model_.lastPicture()
        .then((picture) => {
          if (picture !== this.lastPicture_) {
            this.lastPicture_ = picture;
            return true;
          }
          return false;
        })
        .then((changed) => {
          if (!changed) {
            return;
          }
          this.button_.hidden = !this.lastPicture_;
          var url = this.lastPicture_ && this.lastPicture_.thumbnailURL;
          this.button_.style.backgroundImage =
              url ? ('url("' + url + '")') : 'none';
        });
  }

  /**
   * Opens the gallery to browse the picture.
   * @param {cca.models.Gallery.Picture} picture Picture to be browsed.
   * @private
   */
  openGallery_(picture) {
    const id = 'nlkncpkkdoccmpiclbokaimcnedabhhm|app|open';
    const entry = picture.pictureEntry;
    chrome.fileManagerPrivate.executeTask(id, [entry], (result) => {
      if (chrome.runtime.lastError) {
        console.warn(
            'Unable to open picture: ' + chrome.runtime.lastError.message);
        return;
      }
      if (result !== 'opened' && result !== 'message_sent') {
        console.warn('Unable to open picture: ' + result);
      }
    });
  }

  /**
   * @override
   */
  onPictureDeleted(picture) {
    if (this.lastPicture_ === picture) {
      this.updateButton_();
    }
  }

  /**
   * @override
   */
  onPictureAdded(picture) {
    if (!this.lastPicture_ ||
        this.lastPicture_.timestamp <= picture.timestamp) {
      this.updateButton_();
    }
  }
};
