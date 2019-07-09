// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for the background page.
 */
cca.bg = {};

/**
 * Fixed minimum width of the window inner-bounds in pixels.
 * @type {number}
 * @const
 */
cca.bg.MIN_WIDTH = 768;

/**
 * Initial apsect ratio of the window inner-bounds.
 * @type {number}
 * @const
 */
cca.bg.INITIAL_ASPECT_RATIO = 1.7777777777;

/**
 * Top bar color of the window.
 * @type {string}
 * @const
 */
cca.bg.TOPBAR_COLOR = '#000000';

/**
 * Whether the main AppWindow is created. It's used in test to ensure that we
 * won't connect to the main.html target before the window is created, otherwise
 * the window might disappear.
 * @type {boolean}
 * @deprecated This flag would be removed after we migrate CCA Tast tests.
 */
cca.bg.appWindowCreated = false;

/**
 * It's used in test to ensure that we won't connect to the main.html target
 * before the window is created, otherwise the window might disappear.
 * @type {?function(): undefined}
 */
cca.bg.onAppWindowCreatedForTesting = null;

/**
 * Creates the window. Note, that only one window at once is supported.
 */
cca.bg.create = function() {
  new Promise((resolve) => {
    chrome.storage.local.get(
        {maximized: false, fullscreen: false}, (values) => {
          resolve(values.maximized || values.fullscreen);
        });
  }).then((maximized) => {
    // The height will be later calculated to match video aspect ratio once the
    // stream is available.
    var initialHeight = Math.round(
        cca.bg.MIN_WIDTH / cca.bg.INITIAL_ASPECT_RATIO);

    chrome.app.window.create('views/main.html', {
      id: 'main',
      frame: {color: cca.bg.TOPBAR_COLOR},
      hidden: true, // Will be shown from main.js once loaded.
      innerBounds: {
        width: cca.bg.MIN_WIDTH,
        height: initialHeight,
        minWidth: cca.bg.MIN_WIDTH,
        left: Math.round((window.screen.availWidth - cca.bg.MIN_WIDTH) / 2),
        top: Math.round((window.screen.availHeight - initialHeight) / 2),
      },
    }, (inAppWindow) => {
      // Temporary workaround for crbug.com/452737.
      // 'state' option in CreateWindowOptions is ignored when a window is
      // launched with 'hidden' option, so we restore window state here.
      // Don't launch in fullscreen as the topbar might not work as expected.
      if (maximized) {
        inAppWindow.maximize();
      }
      inAppWindow.onClosed.addListener(() => {
        chrome.storage.local.set({maximized: inAppWindow.isMaximized()});
        chrome.storage.local.set({fullscreen: inAppWindow.isFullscreen()});
      });
      cca.bg.appWindowCreated = true;
      if (cca.bg.onAppWindowCreatedForTesting !== null) {
        cca.bg.onAppWindowCreatedForTesting();
      }
    });
  });
};

chrome.app.runtime.onLaunched.addListener(cca.bg.create);
