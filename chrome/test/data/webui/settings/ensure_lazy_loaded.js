// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @param {string} pathPrefix Prefix for the path to lazy_load.html */
  function ensureLazyLoaded(pathPrefix) {
    // Only trigger lazy loading, if we are in top-level Settings page.
    // IMPORTANT: This is used when running tests that use the Polymer Bundler
    // (aka vulcanize).
    if (location.href === location.origin + '/') {
      suiteSetup(function() {
        return forceLazyLoaded(pathPrefix);
      });
    }
  }

  /**
   * @param {string=} pathPrefix Prefix for the path to lazy_load.html
   * @return {!Promise} Forces the lazy_load.html module to be loaded. No-op if
   *     it has been loaded already.
   */
  function forceLazyLoaded(pathPrefix) {
    return new Promise(function(resolve, reject) {
      // This URL needs to match the location of lazy_load.html (differs across
      // chrome://settings and chrome://os-settings).
      Polymer.Base.importHref(
          `${pathPrefix || ''}/lazy_load.html`, resolve, reject, true);
    });
  }

  return {
    ensureLazyLoaded: ensureLazyLoaded,
    forceLazyLoaded: forceLazyLoaded,
  };
});
