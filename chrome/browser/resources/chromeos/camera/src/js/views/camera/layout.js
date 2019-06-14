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
 * Creates a controller to handle layouts of Camera view.
 * @constructor
 */
cca.views.camera.Layout = function() {
  /**
   * CSS style of the viewport in square mode.
   * @type {CSSStyleDeclaration}
   * @private
   */
  this.squareViewport_ =
      cca.views.camera.Layout.cssStyle_('body.square-preview #preview-wrapper');

  /**
   * CSS style of the video in square mode.
   * @type {CSSStyleDeclaration}
   * @private
   */
  this.squareVideo_ =
      cca.views.camera.Layout.cssStyle_('body.square-preview #preview-video');

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * CSS rules.
 * @type {Array<CSSRule>}
 * @private
 */
cca.views.camera.Layout.cssRules_ =
    [].slice.call(document.styleSheets[0].cssRules);

/**
 * Gets the CSS style by the given selector.
 * @param {string} selector Selector text.
 * @return {CSSStyleDeclaration}
 * @private
 */
cca.views.camera.Layout.cssStyle_ = function(selector) {
  return cca.views.camera.Layout.cssRules_.find(
      (rule) => rule.selectorText == selector).style;
};

/**
 * Updates the video element size for previewing in the window.
 * @return {Array<number>} Letterbox size in [width, height].
 * @private
 */
cca.views.camera.Layout.prototype.updatePreviewSize_ = function() {
  // Make video content keeps its aspect ratio inside the window's inner-bounds;
  // it may fill up the window or be letterboxed when fullscreen/maximized.
  // Don't use app-window.innerBounds' width/height properties during resizing
  // as they are not updated immediately.
  var video = document.querySelector('#preview-video');
  if (video.videoHeight) {
    var scale = cca.state.get('square-mode') ?
        Math.min(window.innerHeight, window.innerWidth) /
            Math.min(video.videoHeight, video.videoWidth) :
        Math.min(
            window.innerHeight / video.videoHeight,
            window.innerWidth / video.videoWidth);
    video.width = scale * video.videoWidth;
    video.height = scale * video.videoHeight;
  }
  var [viewportW, viewportH] = [video.width, video.height];
  cca.state.set('square-preview', cca.state.get('square-mode'));
  if (cca.state.get('square-mode')) {
    viewportW = viewportH = Math.min(video.width, video.height);
    this.squareVideo_.setProperty('left', `${(viewportW - video.width) / 2}px`);
    this.squareVideo_.setProperty('top', `${(viewportH - video.height) / 2}px`);
    this.squareViewport_.setProperty('width', `${viewportW}px`);
    this.squareViewport_.setProperty('height', `${viewportH}px`);
  }
  return [window.innerWidth - viewportW, window.innerHeight - viewportH];
};

/**
 * Updates the layout for video-size or window-size changes.
 */
cca.views.camera.Layout.prototype.update = function() {
  var fullWindow = cca.util.isWindowFullSize();
  var tall = window.innerHeight > window.innerWidth;
  var tabletLandscape = fullWindow && !tall;
  cca.state.set('tablet-landscape', tabletLandscape);
  cca.state.set('max-wnd', fullWindow);
  cca.state.set('tall', tall);

  var [letterboxW, letterboxH] = this.updatePreviewSize_();
  var isLetterboxW = letterboxH < letterboxW;

  cca.state.set('w-letterbox', isLetterboxW);
  if (isLetterboxW) {
    var modeWidth =
        document.querySelector('#modes-group').getBoundingClientRect().width;
    var layoutToggled = false;
    [[modeWidth + 30, 'w-letterbox-s'],
     [modeWidth + 30 + 72, 'w-letterbox-m'],
     [(modeWidth + 30) * 2, 'w-letterbox-l'],
     [Infinity, 'w-letterbox-xl'],
    ]
        .forEach(
            ([wSize, classname]) => cca.state.set(
                classname,
                /* Enable only state which the letterboxW size falls in range
                 * of its wSize and previous wSize. And disable all other
                 * states. */
                !layoutToggled && (layoutToggled = letterboxW <= wSize)));
  } else {
    // preview-vertical-dock: Dock bottom line of preview between gallery and
    //                        mode selector.
    // otherwise: Vertically center the preview.
    cca.state.set('preview-vertical-dock', letterboxH / 2 >= 112);
  }
};
