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
     * @type {!cca.device.PhotoConstraintsPreferrer}
     * @private
     */
    this.photoPreferrer_ = new cca.device.PhotoConstraintsPreferrer(
        () => this.cameraView_.start());

    /**
     * @type {!cca.device.VideoConstraintsPreferrer}
     * @private
     */
    this.videoPreferrer_ = new cca.device.VideoConstraintsPreferrer(
        () => this.cameraView_.start());

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
    this.galleryButton_ = new cca.GalleryButton();

    /**
     * @type {!cca.views.Camera}
     * @private
     */
    this.cameraView_ = (() => {
      const intent = this.backgroundOps_.getIntent();
      const perfLogger = this.backgroundOps_.getPerfLogger();
      if (intent !== null && intent.shouldHandleResult) {
        cca.state.set(cca.state.State.SHOULD_HANDLE_INTENT_RESULT, true);
        return new cca.views.CameraIntent(
            intent, this.infoUpdater_, this.photoPreferrer_,
            this.videoPreferrer_, perfLogger);
      } else {
        const mode = intent !== null ? intent.mode : cca.Mode.PHOTO;
        return new cca.views.Camera(
            this.galleryButton_, this.infoUpdater_, this.photoPreferrer_,
            this.videoPreferrer_, mode, perfLogger);
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
      new cca.views.BaseSettings(cca.views.ViewName.GRID_SETTINGS),
      new cca.views.BaseSettings(cca.views.ViewName.TIMER_SETTINGS),
      new cca.views.ResolutionSettings(
          this.infoUpdater_, this.photoPreferrer_, this.videoPreferrer_),
      new cca.views.BaseSettings(cca.views.ViewName.PHOTO_RESOLUTION_SETTINGS),
      new cca.views.BaseSettings(cca.views.ViewName.VIDEO_RESOLUTION_SETTINGS),
      new cca.views.BaseSettings(cca.views.ViewName.EXPERT_SETTINGS),
      new cca.views.Warning(),
      new cca.views.Dialog(cca.views.ViewName.MESSAGE_DIALOG),
    ]);

    this.backgroundOps_.bindForegroundOps(this);
  }

  /**
   * Sets up toggles (checkbox and radio) by data attributes.
   * @private
   */
  setupToggles_() {
    cca.proxy.browserProxy.localStorageGet(
        {expert: false},
        ({expert}) => cca.state.set(cca.state.State.EXPERT, expert));
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
          cca.state.set(
              cca.state.assertState(element.dataset.state), element.checked);
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
                cca.assertInstanceof(element, HTMLInputElement),
                values[element.dataset.key]));
      }
    });
  }

  /**
   * Starts the app by loading the model and opening the camera-view.
   * @return {!Promise}
   */
  async start() {
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
          const externalDir = cca.models.FileSystem.getExternalDirectory();
          cca.assert(externalDir !== null);
          this.galleryButton_.initialize(externalDir);
          cca.nav.open(cca.views.ViewName.CAMERA);
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
    const showWindow = (async () => {
      await cca.util.fitWindow();
      chrome.app.window.current().show();
      this.backgroundOps_.notifyActivation();
    })();
    const startCamera = (async () => {
      const isSuccess = await this.cameraView_.start();
      this.backgroundOps_.getPerfLogger().stopLaunch({hasError: !isSuccess});
    })();
    return Promise.all([showWindow, startCamera]);
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
    cca.state.set(cca.state.State.SUSPEND, true);
    await this.cameraView_.start();
    chrome.app.window.current().hide();
    this.backgroundOps_.notifySuspension();
  }

  /**
   * Resumes app from suspension and shows app window.
   */
  resume() {
    cca.state.set(cca.state.State.SUSPEND, false);
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
  cca.assert(window['backgroundOps'] !== undefined);
  const /** !cca.bg.BackgroundOps */ bgOps = window['backgroundOps'];
  const perfLogger = bgOps.getPerfLogger();

  // Setup listener for performance events.
  perfLogger.addListener((event, duration, extras) => {
    cca.metrics.log(cca.metrics.Type.PERF, event, duration, extras);
  });
  const states = Object.values(cca.perf.PerfEvent);
  states.push(cca.state.State.TAKING);
  states.forEach((state) => {
    cca.state.addObserver(state, (val, extras) => {
      let event = state;
      if (state === cca.state.State.TAKING) {
        // 'taking' state indicates either taking photo or video. Skips for
        // video-taking case since we only want to collect the metrics of
        // photo-taking.
        if (cca.state.get(cca.Mode.VIDEO)) {
          return;
        }
        event = cca.perf.PerfEvent.PHOTO_TAKING;
      }

      if (val) {
        perfLogger.start(event);
      } else {
        perfLogger.stop(event, extras);
      }
    });
  });
  cca.App.instance_ = new cca.App(
      /** @type {!cca.bg.BackgroundOps} */ (bgOps));
  await cca.App.instance_.start();
});
