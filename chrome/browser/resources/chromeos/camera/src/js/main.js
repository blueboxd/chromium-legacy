// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Creates the Camera App main object.
 * @constructor
 */
cca.App = function() {
  /**
   * @type {cca.models.Gallery}
   * @private
   */
  this.model_ = new cca.models.Gallery();

  /**
   * @type {cca.ResolutionEventBroker}
   * @private
   */
  this.resolBroker_ = new cca.ResolutionEventBroker();

  /**
   * @type {cca.device.PhotoResolPreferrer}
   * @private
   */
  this.photoPreferrer_ = new cca.device.PhotoResolPreferrer(
      this.resolBroker_, () => this.cameraView_.restart());

  /**
   * @type {cca.device.VideoConstraintsPreferrer}
   * @private
   */
  this.videoPreferrer_ = new cca.device.VideoConstraintsPreferrer(
      this.resolBroker_, () => this.cameraView_.restart());

  /**
   * @type {cca.device.DeviceInfoUpdater}
   * @private
   */
  this.infoUpdater_ = new cca.device.DeviceInfoUpdater(
      this.photoPreferrer_, this.videoPreferrer_);

  /**
   * @type {cca.GalleryButton}
   * @private
   */
  this.galleryButton_ = new cca.GalleryButton(this.model_);

  /**
   * @type {cca.views.Browser}
   * @private
   */
  this.browserView_ = new cca.views.Browser(this.model_);

  /**
   * @type {cca.views.Camera}
   * @private
   */
  this.cameraView_ = new cca.views.Camera(
      this.model_, this.infoUpdater_, this.photoPreferrer_,
      this.videoPreferrer_);

  // End of properties. Seal the object.
  Object.seal(this);

  document.body.addEventListener('keydown', this.onKeyPressed_.bind(this));

  document.title = chrome.i18n.getMessage('name');
  cca.util.setupI18nElements(document);
  this.setupToggles_();

  // Set up views navigation by their DOM z-order.
  cca.nav.setup([
    this.cameraView_,
    new cca.views.MasterSettings(),
    new cca.views.BaseSettings('#gridsettings'),
    new cca.views.BaseSettings('#timersettings'),
    new cca.views.ResolutionSettings(
        this.infoUpdater_, this.photoPreferrer_, this.videoPreferrer_,
        this.resolBroker_),
    new cca.views.BaseSettings('#photoresolutionsettings'),
    new cca.views.BaseSettings('#videoresolutionsettings'),
    this.browserView_,
    new cca.views.Warning(),
    new cca.views.Dialog('#message-dialog'),
  ]);
};

/*
 * Checks if it is applicable to use CrOS gallery app.
 * @return {boolean} Whether applicable or not.
 */
cca.App.useGalleryApp = function() {
  return chrome.fileManagerPrivate && cca.state.get('ext-fs');
};

/**
 * Sets up toggles (checkbox and radio) by data attributes.
 * @private
 */
cca.App.prototype.setupToggles_ = function() {
  document.querySelectorAll('input').forEach((element) => {
    element.addEventListener('keypress', (event) =>
        cca.util.getShortcutIdentifier(event) == 'Enter' && element.click());

    var css = element.getAttribute('data-state');
    var key = element.getAttribute('data-key');
    var payload = () => {
      var keys = {};
      keys[key] = element.checked;
      return keys;
    };
    element.addEventListener('change', (event) => {
      if (css) {
        cca.state.set(css, element.checked);
      }
      if (event.isTrusted) {
        element.save();
        if (element.type == 'radio' && element.checked) {
          // Handle unchecked grouped sibling radios.
          var grouped = `input[type=radio][name=${element.name}]:not(:checked)`;
          document.querySelectorAll(grouped).forEach((radio) =>
              radio.dispatchEvent(new Event('change')) && radio.save());
        }
      }
    });
    element.toggleChecked = (checked) => {
      element.checked = checked;
      element.dispatchEvent(new Event('change')); // Trigger toggling css.
    };
    element.save = () => {
      return key && cca.proxy.browserProxy.localStorageSet(payload());
    };
    if (key) {
      // Restore the previously saved state on startup.
      cca.proxy.browserProxy.localStorageGet(
          payload(), (values) => element.toggleChecked(values[key]));
    }
  });
};

/**
 * Starts the app by loading the model and opening the camera-view.
 */
cca.App.prototype.start = function() {
  var ackMigrate = false;
  cca.models.FileSystem.initialize(() => {
    // Prompt to migrate pictures if needed.
    var message = chrome.i18n.getMessage('migrate_pictures_msg');
    return cca.nav.open('message-dialog', {message, cancellable: false})
        .then((acked) => {
          if (!acked) {
            throw new Error('no-migrate');
          }
          ackMigrate = true;
        });
  }).then((external) => {
    cca.state.set('ext-fs', external);
    this.model_.addObserver(this.galleryButton_);
    if (!cca.App.useGalleryApp()) {
      this.model_.addObserver(this.browserView_);
    }
    this.model_.load();
    cca.nav.open('camera');
  }).catch((error) => {
    console.error(error);
    if (error && error.message == 'no-migrate') {
      chrome.app.window.current().close();
      return;
    }
    cca.nav.open('warning', 'filesystem-failure');
  }).finally(() => {
    cca.metrics.log(cca.metrics.Type.LAUNCH, ackMigrate);
  });
};

/**
 * Handles pressed keys.
 * @param {Event} event Key press event.
 * @private
 */
cca.App.prototype.onKeyPressed_ = function(event) {
  cca.tooltip.hide(); // Hide shown tooltip on any keypress.
  cca.nav.onKeyPressed(event);
};

/**
 * @type {cca.App} Singleton of the App object.
 * @private
 */
cca.App.instance_ = null;

/**
 * Creates the App object and starts camera stream.
 */
document.addEventListener('DOMContentLoaded', () => {
  if (!cca.App.instance_) {
    cca.App.instance_ = new cca.App();
  }
  cca.App.instance_.start();
  chrome.app.window.current().show();
});
