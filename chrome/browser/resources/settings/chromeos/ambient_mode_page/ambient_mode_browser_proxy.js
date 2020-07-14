// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// #import {AmbientModeTopicSource, AmbientModeSettings} from './constants.m.js';

/**
 * @fileoverview A helper object used from the ambient mode section to interact
 * with the browser.
 */

cr.define('settings', function() {
  /** @interface */
  /* #export */ class AmbientModeBrowserProxy {
    /**
     * Retrieves the initial settings from server, such as topic source. As a
     * response, the C++ sends the 'topic-source-changed' WebUIListener event.
     */
    onAmbientModePageReady() {}

    /**
     * Updates the selected topic source to server.
     * @param {!AmbientModeTopicSource} topicSource the selected topic source.
     */
    setSelectedTopicSource(topicSource) {}

    /**
     * Retrieves the personal album and art categories from server. As a
     * response, the C++ sends the 'photos-containers-changed' WebUIListener
     * event.
     * @param {!AmbientModeTopicSource} topicSource the topic source for which
     *     the containers requested for.
     */
    requestPhotosContainers(topicSource) {}

    /**
     * Updates the selected personal albums or art categories to server.
     * @param {!AmbientModeSettings} settings the selected albums or categeries.
     */
    setSelectedPhotosContainers(settings) {}
  }

  /** @implements {settings.AmbientModeBrowserProxy} */
  /* #export */ class AmbientModeBrowserProxyImpl {
    /** @override */
    onAmbientModePageReady() {
      chrome.send('onAmbientModePageReady');
    }

    /** @override */
    setSelectedTopicSource(topicSource) {
      chrome.send('setSelectedTopicSource', [topicSource]);
    }

    /** @override */
    requestPhotosContainers(topicSource) {
      chrome.send('requestPhotosContainers', [topicSource]);
    }

    /** @override */
    setSelectedPhotosContainers(settings) {
      chrome.send('setSelectedPhotosContainers', [settings]);
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(AmbientModeBrowserProxyImpl);

  // #cr_define_end
  return {
    AmbientModeBrowserProxy: AmbientModeBrowserProxy,
    AmbientModeBrowserProxyImpl: AmbientModeBrowserProxyImpl,
  };
});
