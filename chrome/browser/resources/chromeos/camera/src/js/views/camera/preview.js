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
 * Namespace for Camera view.
 */
cca.views.camera = cca.views.camera || {};

/**
 * Creates a controller for the video preview of Camera view.
 * @param {function()} onNewStreamNeeded Callback to request new stream.
 * @constructor
 */
cca.views.camera.Preview = function(onNewStreamNeeded) {
  /**
   * @type {function()}
   * @private
   */
  this.onNewStreamNeeded_ = onNewStreamNeeded;

  /**
   * Video element to capture the stream.
   * @type {Video}
   * @private
   */
  this.video_ = document.querySelector('#preview-video');

  /**
   * Current active stream.
   * @type {MediaStream}
   * @private
   */
  this.stream_ = null;

  /**
   * Watchdog for stream-end.
   * @type {?number}
   * @private
   */
  this.watchdog_ = null;

  /**
   * Promise for the current applying focus.
   * @type {?Promise}
   * @private
   */
  this.focus_ = null;

  /**
   * Timeout for resizing the window.
   * @type {?number}
   * @private
   */
  this.resizeWindowTimeout_ = null;

  /**
   * Aspect ratio for the window.
   * @type {number}
   * @private
   */
  this.aspectRatio_ = 1;

  // End of properties, seal the object.
  Object.seal(this);

  var inner = chrome.app.window.current().innerBounds;
  this.aspectRatio_ = inner.width / inner.height;
  window.addEventListener('resize', this.onWindowResize_.bind(this, null));

  this.video_.cleanup = () => {};
};

cca.views.camera.Preview.prototype = {
  get stream() {
    return this.stream_;
  },
};

/**
 * @override
 */
cca.views.camera.Preview.prototype.toString = function() {
  return this.video_.videoHeight ?
      (this.video_.videoWidth + ' x ' + this.video_.videoHeight) : '';
};

/**
 * Sets video element's source.
 * @param {MediaStream} stream Stream to be the source.
 * @return {!Promise} Promise for the operation.
 */
cca.views.camera.Preview.prototype.setSource_ = function(stream) {
  var video = document.createElement('video');
  video.id = 'preview-video';
  video.muted = true; // Mute to avoid echo from the captured audio.
  return new Promise((resolve) => {
    var handler = () => {
      video.removeEventListener('canplay', handler);
      resolve();
    };
    video.addEventListener('canplay', handler);
    video.srcObject = stream;
  }).then(() => video.play()).then(() => {
    video.cleanup = () => {
      video.removeAttribute('srcObject');
      video.load();
    };
    this.video_.parentElement.replaceChild(video, this.video_).cleanup();
    this.video_ = video;
    video.addEventListener('resize', () => this.onIntrinsicSizeChanged_());
    video.addEventListener('click', (event) => this.onFocusClicked_(event));
    return this.onIntrinsicSizeChanged_();
  });
};

/**
 * Starts the preview with the source stream.
 * @param {MediaStream} stream Stream to be the source.
 * @return {!Promise} Promise for the operation.
 */
cca.views.camera.Preview.prototype.start = function(stream) {
  return this.setSource_(stream).then(() => {
    // Use a watchdog since the stream.onended event is unreliable in the
    // recent version of Chrome. As of 55, the event is still broken.
    this.watchdog_ = setInterval(() => {
      // Check if video stream is ended (audio stream may still be live).
      if (!stream.getVideoTracks().length ||
          stream.getVideoTracks()[0].readyState == 'ended') {
        clearInterval(this.watchdog_);
        this.watchdog_ = null;
        this.stream_ = null;
        this.onNewStreamNeeded_();
      }
    }, 100);
    this.stream_ = stream;
    cca.state.set('streaming', true);
  });
};

/**
 * Stops the preview.
 */
cca.views.camera.Preview.prototype.stop = function() {
  if (this.watchdog_) {
    clearInterval(this.watchdog_);
    this.watchdog_ = null;
  }
  // Pause video element to avoid black frames during transition.
  this.video_.pause();
  if (this.stream_) {
    this.stream_.getVideoTracks()[0].stop();
    this.stream_ = null;
  }
  cca.state.set('streaming', false);
};

/**
 * Creates an image blob of the current frame.
 * @return {!Promise<Blob>} Promise for the result.
 */
cca.views.camera.Preview.prototype.toImage = function() {
  var canvas = document.createElement('canvas');
  var ctx = canvas.getContext('2d');
  canvas.width = this.video_.videoWidth;
  canvas.height = this.video_.videoHeight;
  ctx.drawImage(this.video_, 0, 0);
  return new Promise((resolve, reject) => {
    canvas.toBlob((blob) => {
      if (blob) {
        resolve(blob);
      } else {
        reject(new Error('Photo blob error.'));
      }
    }, 'image/jpeg');
  });
};

/**
 * Sets CCA window inner bound to specified size.
 * @param {number} width Width and min width of new window inner bound.
 * @param {number} height Height and min height of new window inner bound.
 * @return {Promise} Promise which is resolved when size change actually happen.
 * @private
 */
cca.views.camera.Preview.prototype.setWindowSize_ = function(width, height) {
  return new Promise((resolve) => {
    const listener = () => {
      chrome.app.window.current().onBoundsChanged.removeListener(listener);
      resolve();
    };
    chrome.app.window.current().onBoundsChanged.addListener(listener);
    const inner = chrome.app.window.current().innerBounds;
    const prevW = inner.width;
    const prevH = inner.height;
    inner.minWidth = inner.width = width;
    inner.minHeight = inner.height = height;
    if (prevW == width && prevH == height) {
      listener();
    }
  });
};

/**
 * Handles resizing the window for preview's aspect ratio changes.
 * @param {number=} aspectRatio Aspect ratio changed.
 * @return {Promise}
 * @private
 */
cca.views.camera.Preview.prototype.onWindowResize_ = function(aspectRatio) {
  if (this.resizeWindowTimeout_) {
    clearTimeout(this.resizeWindowTimeout_);
    this.resizeWindowTimeout_ = null;
  }
  cca.nav.onWindowResized();

  // Resize window for changed preview's aspect ratio or restore window size by
  // the last known window's aspect ratio.
  return new Promise((resolve) => {
           if (aspectRatio) {
             this.aspectRatio_ = aspectRatio;
             resolve();
           } else {
             this.resizeWindowTimeout_ = setTimeout(() => {
               this.resizeWindowTimeout_ = null;
               resolve();
             }, 500);  // Delay further resizing for smooth UX.
           }
         })
      .then(() => {
        // Resize window by aspect ratio only if it's not maximized or
        // fullscreen.
        if (cca.util.isWindowFullSize()) {
          return;
        }
        const width = chrome.app.window.current().innerBounds.minWidth;
        const height = Math.round(width * 9 / 16);
        return this.setWindowSize_(width, height);
      });
};

/**
 * Handles changed intrinsic size (first loaded or orientation changes).
 * @async
 * @private
 */
cca.views.camera.Preview.prototype.onIntrinsicSizeChanged_ = async function() {
  if (this.video_.videoWidth && this.video_.videoHeight) {
    await this.onWindowResize_(
        this.video_.videoWidth / this.video_.videoHeight);
  }
  this.cancelFocus_();
};

/**
 * Handles clicking for focus.
 * @param {Event} event Click event.
 * @private
 */
cca.views.camera.Preview.prototype.onFocusClicked_ = function(event) {
  this.cancelFocus_();

  // Normalize to square space coordinates by W3C spec.
  var x = event.offsetX / this.video_.width;
  var y = event.offsetY / this.video_.height;
  var constraints = {advanced: [{pointsOfInterest: [{x, y}]}]};
  var track = this.video_.srcObject.getVideoTracks()[0];
  var focus = track.applyConstraints(constraints).then(() => {
    if (focus != this.focus_) {
      return; // Focus was cancelled.
    }
    var aim = document.querySelector('#preview-focus-aim');
    var clone = aim.cloneNode(true);
    clone.style.left = `${event.offsetX + this.video_.offsetLeft}px`;
    clone.style.top = `${event.offsetY + this.video_.offsetTop}px`;
    clone.hidden = false;
    aim.parentElement.replaceChild(clone, aim);
  }).catch(console.error);
  this.focus_ = focus;
};

/**
 * Cancels the current applying focus.
 * @private
 */
cca.views.camera.Preview.prototype.cancelFocus_ = function() {
  this.focus_ = null;
  document.querySelector('#preview-focus-aim').hidden = true;
};
