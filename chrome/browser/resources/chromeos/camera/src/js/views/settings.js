// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Tuple of device id, width, height of preferred capture resolution and all
 * available resolutions for a specific video device.
 * @typedef {[string, number, number, ResolList]} DeviceIdResols
 */

/**
 * Creates the base controller of settings view.
 * @param {string} selector Selector text of the view's root element.
 * @param {Object<string|function(Event=)>=} itemHandlers Click-handlers
 *     mapped by element ids.
 * @extends {cca.views.View}
 * @constructor
 */
cca.views.BaseSettings = function(selector, itemHandlers = {}) {
  cca.views.View.call(this, selector, true, true);

  this.root.querySelector('.menu-header button').addEventListener(
      'click', () => this.leave());
  this.root.querySelectorAll('.menu-item').forEach((element) => {
    var handler = itemHandlers[element.id];
    if (handler) {
      element.addEventListener('click', handler);
    }
  });
};

cca.views.BaseSettings.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * @override
 */
cca.views.BaseSettings.prototype.focus = function() {
  this.rootElement_.querySelector('[tabindex]').focus();
};

/**
 * Opens sub-settings.
 * @param {string} id Settings identifier.
 * @private
 */
cca.views.BaseSettings.prototype.openSubSettings = function(id) {
  // Dismiss master-settings if sub-settings was dimissed by background click.
  cca.nav.open(id).then((cond) => cond && cond.bkgnd && this.leave());
};

/**
 * Creates the controller of master settings view.
 * @extends {cca.views.BaseSettings}
 * @constructor
 */
cca.views.MasterSettings = function() {
  cca.views.BaseSettings.call(this, '#settings', {
    'settings-gridtype': () => this.openSubSettings('gridsettings'),
    'settings-timerdur': () => this.openSubSettings('timersettings'),
    'settings-resolution': () => this.openSubSettings('resolutionsettings'),
    'settings-feedback': () => this.openFeedback(),
    'settings-help': () => cca.util.openHelp(),
  });

  // End of properties, seal the object.
  Object.seal(this);

  document.querySelector('#settings-feedback').hidden =
      !cca.util.isChromeVersionAbove(72); // Feedback available since M72.
};

cca.views.MasterSettings.prototype = {
  __proto__: cca.views.BaseSettings.prototype,
};

/**
 * Opens feedback.
 * @private
 */
cca.views.MasterSettings.prototype.openFeedback = function() {
  var data = {
    'categoryTag': 'chromeos-camera-app',
    'requestFeedback': true,
    'feedbackInfo': {
      'description': '',
      'systemInformation': [
        {key: 'APP ID', value: chrome.runtime.id},
        {key: 'APP VERSION', value: chrome.runtime.getManifest().version},
      ],
    },
  };
  const id = 'gfdkimpbcpahaombhbimeihdjnejgicl'; // Feedback extension id.
  chrome.runtime.sendMessage(id, data);
};

/**
 * Creates the controller of resolution settings view.
 * @param {!cca.device.DeviceInfoUpdater} infoUpdater
 * @param {!cca.device.PhotoResolPreferrer} photoPreferrer
 * @param {!cca.device.VideoConstraintsPreferrer} videoPreferrer
 * @param {!cca.ResolutionEventBroker} resolBroker
 * @extends {cca.views.BaseSettings}
 * @constructor
 */
cca.views.ResolutionSettings = function(
    infoUpdater, photoPreferrer, videoPreferrer, resolBroker) {
  cca.views.BaseSettings.call(this, '#resolutionsettings', {
    'settings-front-photores': () => {
      const element = document.querySelector('#settings-front-photores');
      if (element.classList.contains('multi-option')) {
        this.openPhotoResSettings_(this.frontPhotoSetting_, element);
      }
    },
    'settings-front-videores': () => {
      const element = document.querySelector('#settings-front-videores');
      if (element.classList.contains('multi-option')) {
        this.openVideoResSettings_(this.frontVideoSetting_, element);
      }
    },
    'settings-back-photores': () => {
      const element = document.querySelector('#settings-back-photores');
      if (element.classList.contains('multi-option')) {
        this.openPhotoResSettings_(this.backPhotoSetting_, element);
      }
    },
    'settings-back-videores': () => {
      const element = document.querySelector('#settings-back-videores');
      if (element.classList.contains('multi-option')) {
        this.openVideoResSettings_(this.backVideoSetting_, element);
      }
    },
  });

  /**
   * @type {cca.ResolutionEventBroker}
   * @private
   */
  this.resolBroker_ = resolBroker;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.resMenu_ = document.querySelector('#resolutionsettings>div.menu');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.videoResMenu_ =
      document.querySelector('#videoresolutionsettings>div.menu');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.photoResMenu_ =
      document.querySelector('#photoresolutionsettings>div.menu');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.frontPhotoItem_ = document.querySelector('#settings-front-photores');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.frontVideoItem_ = document.querySelector('#settings-front-videores');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.backPhotoItem_ = document.querySelector('#settings-back-photores');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.backVideoItem_ = document.querySelector('#settings-back-videores');

  /**
   * @type {HTMLTemplateElement}
   * @private
   */
  this.resItemTempl_ = document.querySelector('#resolution-item-template');

  /**
   * @type {HTMLTemplateElement}
   * @private
   */
  this.extcamItemTempl_ =
      document.querySelector('#extcam-resolution-item-template');

  /**
   * Device id and photo resolutions of front camera. Null if no front camera.
   * @type {?DeviceIdResols}
   * @private
   */
  this.frontPhotoSetting_ = null;

  /**
   * Device id and video resolutions of front camera. Null if no front camera.
   * @type {?DeviceIdResols}
   * @private
   */
  this.frontVideoSetting_ = null;

  /**
   * Device id and photo resolutions of back camera. Null if no back camera.
   * @type {?DeviceIdResols}
   * @private
   */
  this.backPhotoSetting_ = null;

  /**
   * Device id and video resolutions of back camera. Null if no back camera.
   * @type {?DeviceIdResols}
   * @private
   */
  this.backVideoSetting_ = null;

  /**
   * Device id and photo resolutions of external cameras.
   * @type {Array<DeviceIdResols>}
   * @private
   */
  this.externalPhotoSettings_ = [];

  /**
   * Device id and video resolutions of external cameras.
   * @type {Array<DeviceIdResols>}
   * @private
   */
  this.externalVideoSettings_ = [];

  /**
   * Device id of currently opened resolution setting view.
   * @type {?string}
   * @private
   */
  this.openedSettingDeviceId_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  infoUpdater.addDeviceChangeListener(async (updater) => {
    const devices = await updater.getCamera3DevicesInfo();
    if (!devices) {
      cca.state.set('no-resolution-settings', true);
      return;
    }

    this.frontPhotoSetting_ = this.backPhotoSetting_ = null;
    this.externalPhotoSettings_ = [];
    this.frontVideoSetting_ = this.backVideoSetting_ = null;
    this.externalVideoSettings_ = [];

    devices.forEach(({deviceId, facing, photoResols, videoResols}) => {
      // Filter out resolutions of megapixels < 0.1 i.e. megapixels 0.0
      const photoSetting = [
        deviceId, ...photoPreferrer.getPrefResolution(deviceId),
        photoResols.filter(([w, h]) => w * h >= 100000),
      ];
      const videoSetting = [
        deviceId, ...videoPreferrer.getPrefResolution(deviceId), videoResols,
      ];
      switch (facing) {
        case cros.mojom.CameraFacing.CAMERA_FACING_FRONT:
          this.frontPhotoSetting_ = photoSetting;
          this.frontVideoSetting_ = videoSetting;
          break;
        case cros.mojom.CameraFacing.CAMERA_FACING_BACK:
          this.backPhotoSetting_ = photoSetting;
          this.backVideoSetting_ = videoSetting;
          break;
        case cros.mojom.CameraFacing.CAMERA_FACING_EXTERNAL:
          this.externalPhotoSettings_.push(photoSetting);
          this.externalVideoSettings_.push(videoSetting);
          break;
        default:
          console.error(`Ignore device of unknown facing: ${facing}`);
      }
    });
    this.updateResolutions_();
  });

  this.resolBroker_.addPhotoPrefResolChangeListener(
      this.updateSelectedPhotoResolution_.bind(this));
  this.resolBroker_.addVideoPrefResolChangeListener(
      this.updateSelectedVideoResolution_.bind(this));
};

cca.views.ResolutionSettings.prototype = {
  __proto__: cca.views.BaseSettings.prototype,
};

/**
 * Template for generating option text from photo resolution width and height.
 * @param {number} w Resolution width.
 * @param {number} h Resolution height.
 * @param {ResolList} resolutions All available resolutions.
 * @return {string} Text shown on resolution option item.
 * @private
 */
cca.views.ResolutionSettings.prototype.photoOptTextTempl_ = function(
    w, h, resolutions) {
  const gcd = (a, b) => (a <= 0 ? b : gcd(b % a, a));
  const d = gcd(w, h);
  const toMegapixel = (w, h) => Math.round(w * h / 100000) / 10;
  if (resolutions.find(
          ([w2, h2]) => (w != w2 || h != h2) && w * h2 == w2 * h &&
              toMegapixel(w, h) == toMegapixel(w2, h2))) {
    return chrome.i18n.getMessage(
        'label_detail_photo_resolution',
        [w / d, h / d, w, h, toMegapixel(w, h)]);
  } else {
    return chrome.i18n.getMessage(
        'label_photo_resolution', [w / d, h / d, toMegapixel(w, h)]);
  }
};

/**
 * Template for generating option text from video resolution width and height.
 * @param {number} w Resolution width.
 * @param {number} h Resolution height.
 * @return {string} Text shown on resolution option item.
 * @private
 */
cca.views.ResolutionSettings.prototype.videoOptTextTempl_ = function(w, h) {
  return chrome.i18n.getMessage('label_video_resolution', [h, w].map(String));
};

/**
 * Finds photo and video resolution setting of target device id.
 * @param {string} deviceId
 * @return {?[DeviceIdResols, DeviceIdResols]}
 * @private
 */
cca.views.ResolutionSettings.prototype.getDeviceSetting_ = function(deviceId) {
  if (this.frontPhotoSetting_ && this.frontPhotoSetting_[0] === deviceId) {
    return [this.frontPhotoSetting_, this.frontVideoSetting_];
  }
  if (this.backPhotoSetting_ && this.backPhotoSetting_[0] === deviceId) {
    return [this.backPhotoSetting_, this.backVideoSetting_];
  }
  const idx = this.externalPhotoSettings_.findIndex(([id]) => id === deviceId);
  return idx == -1 ?
      null :
      [this.externalPhotoSettings_[idx], this.externalVideoSettings_[idx]];
};

/**
 * Updates resolution information of front, back camera and external cameras.
 * @private
 */
cca.views.ResolutionSettings.prototype.updateResolutions_ = function() {
  const prepItem = (item, [id, w, h, resolutions], optTextTempl) => {
    item.dataset.deviceId = id;
    item.classList.toggle('multi-option', resolutions.length > 1);
    item.querySelector('.description>span').textContent =
        optTextTempl(w, h, resolutions);
  };

  // Update front camera setting
  cca.state.set('has-front-camera', !!this.frontPhotoSetting_);
  if (this.frontPhotoSetting_) {
    prepItem(
        this.frontPhotoItem_, this.frontPhotoSetting_, this.photoOptTextTempl_);
    prepItem(
        this.frontVideoItem_, this.frontVideoSetting_, this.videoOptTextTempl_);
  }

  // Update back camera setting
  cca.state.set('has-back-camera', !!this.backPhotoSetting_);
  if (this.backPhotoSetting_) {
    prepItem(
        this.backPhotoItem_, this.backPhotoSetting_, this.photoOptTextTempl_);
    prepItem(
        this.backVideoItem_, this.backVideoSetting_, this.videoOptTextTempl_);
  }

  // Update external camera settings
  // To prevent losing focus on item already exist before update, locate
  // focused item in both previous and current list, pop out all items in
  // previous list except those having same deviceId as focused one and
  // recreate all other items from current list.
  const prevFocus =
      this.resMenu_.querySelector('.menu-item.external-camera:focus');
  const prevFId = prevFocus && prevFocus.dataset.deviceId;
  const focusIdx =
      this.externalPhotoSettings_.findIndex(([id]) => id === prevFId);
  const fTitle =
      this.resMenu_.querySelector(`.menu-item[data-device-id="${prevFId}"]`);
  const focusedId = focusIdx === -1 ? null : prevFId;

  this.resMenu_.querySelectorAll('.menu-item.external-camera')
      .forEach(
          (element) => element.dataset.deviceId !== focusedId &&
              element.parentNode.removeChild(element));

  this.externalPhotoSettings_.forEach((photoSetting, index) => {
    const deviceId = photoSetting[0];
    const videoSetting = this.externalVideoSettings_[index];
    if (deviceId !== focusedId) {
      const extItem = document.importNode(this.extcamItemTempl_.content, true);
      cca.util.setupI18nElements(extItem);
      var [titleItem, photoItem, videoItem] =
          extItem.querySelectorAll('.menu-item');

      photoItem.addEventListener('click', () => {
        if (photoItem.classList.contains('multi-option')) {
          this.openPhotoResSettings_(photoSetting, photoItem);
        }
      });
      photoItem.setAttribute('aria-describedby', `${deviceId}-photores-desc`);
      photoItem.querySelector('.description').id = `${deviceId}-photores-desc`;
      videoItem.addEventListener('click', () => {
        if (videoItem.classList.contains('multi-option')) {
          this.openVideoResSettings_(videoSetting, videoItem);
        }
      });
      videoItem.setAttribute('aria-describedby', `${deviceId}-videores-desc`);
      videoItem.querySelector('.description').id = `${deviceId}-videores-desc`;
      if (index < focusIdx) {
        this.resMenu_.insertBefore(extItem, fTitle);
      } else {
        this.resMenu_.appendChild(extItem);
      }
    } else {
      titleItem = fTitle;
      photoItem = fTitle.nextElementSibling;
      videoItem = photoItem.nextElementSibling;
    }
    titleItem.dataset.deviceId = deviceId;
    prepItem(photoItem, photoSetting, this.photoOptTextTempl_);
    prepItem(videoItem, videoSetting, this.videoOptTextTempl_);
  });
  if ((cca.state.get('photoresolutionsettings') ||
       cca.state.get('videoresolutionsettings')) &&
      !this.getDeviceSetting_(this.openedSettingDeviceId_)) {
    cca.nav.close(
        cca.state.get('photoresolutionsettings') ? 'photoresolutionsettings' :
                                                   'videoresolutionsettings');
  }
};

/**
 * Updates current selected photo resolution.
 * @param {string} deviceId Device id of the selected resolution.
 * @param {number} width Width of selected resolution.
 * @param {number} height Height of selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.updateSelectedPhotoResolution_ =
    function(deviceId, width, height) {
  const [photoSetting] = this.getDeviceSetting_(deviceId);
  photoSetting[1] = width;
  photoSetting[2] = height;
  if (this.frontPhotoSetting_ && deviceId === this.frontPhotoSetting_[0]) {
    var photoItem = this.frontPhotoItem_;
  } else if (this.backPhotoSetting_ && deviceId === this.backPhotoSetting_[0]) {
    photoItem = this.backPhotoItem_;
  } else {
    photoItem = this.resMenu_.querySelectorAll(
        `.menu-item[data-device-id="${deviceId}"]`)[1];
  }
  photoItem.querySelector('.description>span').textContent =
      this.photoOptTextTempl_(width, height, photoSetting[3]);

  // Update setting option if it's opened.
  if (cca.state.get('photoresolutionsettings') &&
      this.openedSettingDeviceId_ === deviceId) {
    this.photoResMenu_
        .querySelector(`input[data-width="${width}"][data-height="${height}"]`)
        .checked = true;
  }
};

/**
 * Updates current selected video resolution.
 * @param {string} deviceId Device id of the selected resolution.
 * @param {number} width Width of selected resolution.
 * @param {number} height Height of selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.updateSelectedVideoResolution_ =
    function(deviceId, width, height) {
  const [, videoSetting] = this.getDeviceSetting_(deviceId);
  videoSetting[1] = width;
  videoSetting[2] = height;
  if (this.frontVideoSetting_ && deviceId === this.frontVideoSetting_[0]) {
    var videoItem = this.frontVideoItem_;
  } else if (this.backVideoSetting_ && deviceId === this.backVideoSetting_[0]) {
    videoItem = this.backVideoItem_;
  } else {
    videoItem = this.resMenu_.querySelectorAll(
        `.menu-item[data-device-id="${deviceId}"]`)[2];
  }
  videoItem.querySelector('.description>span').textContent =
      this.videoOptTextTempl_(width, height);

  // Update setting option if it's opened.
  if (cca.state.get('videoresolutionsettings') &&
      this.openedSettingDeviceId_ === deviceId) {
    this.videoResMenu_
        .querySelector(`input[data-width="${width}"][data-height="${height}"]`)
        .checked = true;
  }
};

/**
 * Opens photo resolution setting view.
 * @param {DeviceIdResols} setting Photo resolution setting.
 * @param {HTMLElement} resolItem Dom element from upper layer menu item showing
 *     title of the selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.openPhotoResSettings_ = function(
    setting, resolItem) {
  const [deviceId, width, height, resolutions] = setting;
  this.openedSettingDeviceId_ = deviceId;
  this.updateMenu_(
      resolItem, this.photoResMenu_, this.photoOptTextTempl_,
      (w, h) => this.resolBroker_.requestChangePhotoPrefResol(deviceId, w, h),
      resolutions, width, height);
  this.openSubSettings('photoresolutionsettings');
};

/**
 * Opens video resolution setting view.
 * @param {DeviceIdResols} setting Video resolution setting.
 * @param {HTMLElement} resolItem Dom element from upper layer menu item showing
 *     title of the selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.openVideoResSettings_ = function(
    setting, resolItem) {
  const [deviceId, width, height, resolutions] = setting;
  this.openedSettingDeviceId_ = deviceId;
  this.updateMenu_(
      resolItem, this.videoResMenu_, this.videoOptTextTempl_,
      (w, h) => this.resolBroker_.requestChangeVideoPrefResol(deviceId, w, h),
      resolutions, width, height);
  this.openSubSettings('videoresolutionsettings');
};

/**
 * Updates resolution menu with specified resolutions.
 * @param {HTMLElement} resolItem DOM element holding selected resolution.
 * @param {HTMLElement} menu Menu holding all resolution option elements.
 * @param {function(number, number, ResolList): string} optTextTempl Template
 *     generating text content for each resolution option from its
 *     width and height.
 * @param {function(number, number)} onChange Called when selected
 *     option changed with the its width and height
 * @param {ResolList} resolutions Resolutions of its width and height to be
 *      updated with.
 * @param {number} selectedWidth Width of selected resolution.
 * @param {number} selectedHeight Height of selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.updateMenu_ = function(
    resolItem, menu, optTextTempl, onChange, resolutions, selectedWidth,
    selectedHeight) {
  const captionText = resolItem.querySelector('.description>span');
  captionText.textContent = '';
  menu.querySelectorAll('.menu-item')
      .forEach((element) => element.parentNode.removeChild(element));

  resolutions.forEach(([w, h], index) => {
    const item = document.importNode(this.resItemTempl_.content, true);
    const inputElement = item.querySelector('input');
    item.querySelector('span').textContent = optTextTempl(w, h, resolutions);
    inputElement.name = menu.dataset.name;
    inputElement.dataset.width = w;
    inputElement.dataset.height = h;
    if (w == selectedWidth && h == selectedHeight) {
      captionText.textContent = optTextTempl(w, h, resolutions);
      inputElement.checked = true;
    }
    inputElement.addEventListener('click', (event) => {
      if (!cca.state.get('streaming') || cca.state.get('taking')) {
        event.preventDefault();
      }
    });
    inputElement.addEventListener('change', (event) => {
      if (inputElement.checked) {
        captionText.textContent = optTextTempl(w, h, resolutions);
        onChange(w, h);
      }
    });
    menu.appendChild(item);
  });
};
