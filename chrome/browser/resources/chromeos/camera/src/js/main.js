// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * import {assert, assertInstanceof} from './chrome_util.js';
 */
var {assert, assertInstanceof} = {assert, assertInstanceof};

/**
 * import {Mode} from './type.js';
 */
var Mode = Mode || {};

/**
 * Creates the Camera App main object.
 * @implements {cca.bg.ForegroundOps}
 */
cca.App = class {
  /**
   * @param {!cca.bg.BackgroundOps} backgroundOps
   */
  constructor(backgroundOps) {
    /**
     * @type {!cca.bg.BackgroundOps}
     * @private
     */
    this.backgroundOps_ = backgroundOps;

    /**
     * @type {!cca.models.Gallery}
     * @private
     */
    this.gallery_ = new cca.models.Gallery();

    /**
     * @type {!cca.device.PhotoConstraintsPreferrer}
     * @private
     */
    this.photoPreferrer_ = new cca.device.PhotoConstraintsPreferrer(
        () => this.cameraView_.restart());

    /**
     * @type {!cca.device.VideoConstraintsPreferrer}
     * @private
     */
    this.videoPreferrer_ = new cca.device.VideoConstraintsPreferrer(
        () => this.cameraView_.restart());

    /**
     * @type {!cca.device.DeviceInfoUpdater}
     * @private
     */
    this.infoUpdater_ = new cca.device.DeviceInfoUpdater(
        this.photoPreferrer_, this.videoPreferrer_);

    /**
     * @type {!cca.GalleryButton}
     * @private
     */
    this.galleryButton_ = new cca.GalleryButton(this.gallery_);

    /**
     * @type {!cca.views.Camera}
     * @private
     */
    this.cameraView_ = (() => {
      const intent = this.backgroundOps_.getIntent();
      if (intent !== null && intent.shouldHandleResult) {
        cca.state.set('should-handle-intent-result', true);
        return new cca.views.CameraIntent(
            intent, this.infoUpdater_, this.photoPreferrer_,
            this.videoPreferrer_);
      } else {
        return new cca.views.Camera(
            this.gallery_, this.infoUpdater_, this.photoPreferrer_,
            this.videoPreferrer_, intent !== null ? intent.mode : Mode.PHOTO);
      }
    })();

    document.body.addEventListener('keydown', this.onKeyPressed_.bind(this));

    document.title = chrome.i18n.getMessage('name');
    cca.util.setupI18nElements(document.body);
    this.setupToggles_();

    // Set up views navigation by their DOM z-order.
    cca.nav.setup([
      this.cameraView_,
      new cca.views.MasterSettings(),
      new cca.views.BaseSettings('#gridsettings'),
      new cca.views.BaseSettings('#timersettings'),
      new cca.views.ResolutionSettings(
          this.infoUpdater_, this.photoPreferrer_, this.videoPreferrer_),
      new cca.views.BaseSettings('#photoresolutionsettings'),
      new cca.views.BaseSettings('#videoresolutionsettings'),
      new cca.views.BaseSettings('#expertsettings'),
      new cca.views.Warning(),
      new cca.views.Dialog('#message-dialog'),
    ]);

    this.backgroundOps_.bindForegroundOps(this);
  }

  /**
   * Sets up toggles (checkbox and radio) by data attributes.
   * @private
   */
  setupToggles_() {
    cca.proxy.browserProxy.localStorageGet(
        {expert: false}, ({expert}) => cca.state.set('expert', expert));
    document.querySelectorAll('input').forEach((element) => {
      element.addEventListener(
          'keypress',
          (event) => cca.util.getShortcutIdentifier(event) === 'Enter' &&
              element.click());

      const payload = (element) => ({[element.dataset.key]: element.checked});
      const save = (element) => {
        if (element.dataset.key !== undefined) {
          cca.proxy.browserProxy.localStorageSet(payload(element));
        }
      };
      element.addEventListener('change', (event) => {
        if (element.dataset.state !== undefined) {
          cca.state.set(element.dataset.state, element.checked);
        }
        if (event.isTrusted) {
          save(element);
          if (element.type === 'radio' && element.checked) {
            // Handle unchecked grouped sibling radios.
            const grouped =
                `input[type=radio][name=${element.name}]:not(:checked)`;
            document.querySelectorAll(grouped).forEach(
                (radio) =>
                    radio.dispatchEvent(new Event('change')) && save(radio));
          }
        }
      });
      if (element.dataset.key !== undefined) {
        // Restore the previously saved state on startup.
        cca.proxy.browserProxy.localStorageGet(
            payload(element),
            (values) => cca.util.toggleChecked(
                assertInstanceof(element, HTMLInputElement),
                values[element.dataset.key]));
      }
    });
  }

  /**
   * Starts the app by loading the model and opening the camera-view.
   */
  start() {
    var ackMigrate = false;
    cca.models.FileSystem
        .initialize(() => {
          // Prompt to migrate pictures if needed.
          var message = chrome.i18n.getMessage('migrate_pictures_msg');
          return cca.nav.open('message-dialog', {message, cancellable: false})
              .then((acked) => {
                if (!acked) {
                  throw new Error('no-migrate');
                }
                ackMigrate = true;
              });
        })
        .then((external) => {
          cca.state.set('ext-fs', external);
          this.gallery_.addObserver(this.galleryButton_);
          this.gallery_.load();
          cca.nav.open('camera');
        })
        .catch((error) => {
          console.error(error);
          if (error && error.message === 'no-migrate') {
            chrome.app.window.current().close();
            return;
          }
          cca.nav.open('warning', 'filesystem-failure');
        })
        .finally(() => {
          cca.metrics.log(cca.metrics.Type.LAUNCH, ackMigrate);
        });
    chrome.app.window.current().show();
    this.backgroundOps_.notifyActivation();
  }

  /**
   * Handles pressed keys.
   * @param {Event} event Key press event.
   * @private
   */
  onKeyPressed_(event) {
    cca.tooltip.hide();  // Hide shown tooltip on any keypress.
    cca.nav.onKeyPressed(event);
  }

  /**
   * Suspends app and hides app window.
   * @return {!Promise}
   */
  async suspend() {
    cca.state.set('suspend', true);
    await this.cameraView_.restart();
    chrome.app.window.current().hide();
    this.backgroundOps_.notifySuspension();
  }

  /**
   * Resumes app from suspension and shows app window.
   */
  resume() {
    cca.state.set('suspend', false);
    chrome.app.window.current().show();
    this.backgroundOps_.notifyActivation();
  }
};

/**
 * Singleton of the App object.
 * @type {?cca.App}
 * @private
 */
cca.App.instance_ = null;

/**
 * Creates the App object and starts camera stream.
 */
document.addEventListener('DOMContentLoaded', async () => {
  if (cca.App.instance_ !== null) {
    return;
  }
  assert(window['backgroundOps'] !== undefined);
  cca.App.instance_ = new cca.App(
      /** @type {!cca.bg.BackgroundOps} */ (window['backgroundOps']));
  cca.App.instance_.start();
});
