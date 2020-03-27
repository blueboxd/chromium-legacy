// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of SettingsSearchHandler for testing.
 */
cr.define('settings', function() {
  /**
   * Fake implementation of chromeos.settings.mojom.SettingsSearchHandlerRemote.
   */
  class FakeSettingsSearchHandler {
    constructor() {
      /** @private {!Array<string>} */
      this.fakeResults_ = [];
    }

    /**
     * @param {!Array<string>} results fake results that will be returned
     * when Search() is called.
     */
    setFakeResults(results) {
      this.fakeResults_ = results;
    }

    /**
     * @param {string} query fake query used to compile search results.
     */
    async search(query) {
      return this.fakeResults_;
    }
  }

  return {FakeSettingsSearchHandler: FakeSettingsSearchHandler};
});
