// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/origin.mojom-lite.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {ModuleDescriptor} from '../module_descriptor.js';

// TODO(beccahughes): Use import for these.
const KALEIDOSCOPE_RESOURCES = [
  'chrome://kaleidoscope/geometry.mojom-lite.js',
  'chrome://kaleidoscope/chrome/browser/media/feeds/media_feeds_store.mojom-lite.js',
  'chrome://kaleidoscope/kaleidoscope.mojom-lite.js',
  'chrome://kaleidoscope/messages.js',
  'chrome://kaleidoscope/kaleidoscope.js',
  'chrome://kaleidoscope/module.js',
];

/**
 * Loads a script resource and returns a promise that will resolve when the
 * loading is complete.
 * @param {string} resource
 * @returns {Promise}
 */
function loadResource(resource) {
  return new Promise((resolve) => {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = resource;
    script.addEventListener('load', resolve, {once: true});
    document.body.appendChild(script);
  });
}

/** @type {!ModuleDescriptor} */
export const kaleidoscopeDescriptor = new ModuleDescriptor(
    'kaleidoscope',
    loadTimeData.getString('modulesKaleidoscopeName'),
    () => {
      // Load all the Kaleidoscope resources into the NTP and return the module
      // once the loading is complete.
      return Promise.all(KALEIDOSCOPE_RESOURCES.map((r) => loadResource(r)))
          .then(() => {
            return {
              element: document.createElement('ntp-kaleidoscope-module'),
              title: loadTimeData.getString('modulesKaleidoscopeTitle'),
            };
          });
    },
);
