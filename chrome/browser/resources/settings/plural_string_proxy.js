// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used to get a pluralized string.
 */

// clang-format off
// #import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

cr.define('settings', function() {
  /** @interface */
  class PluralStringProxy {
    /**
     * Obtains a pluralized string for |messageName| with |itemCount| items.
     * @param {!string} messageName The name of the message.
     * @param {!number} itemCount The number of items.
     * @return {!Promise<string>} Promise resolved with the appropriate plural
     *     string for |messageName| with |itemCount| items.
     */
    getPluralString(messageName, itemCount) {}
  }

  /** @implements {settings.PluralStringProxy} */
  /* #export */ class PluralStringProxyImpl {
    /** @override */
    getPluralString(messageName, itemCount) {
      return cr.sendWithPromise('getPluralString', messageName, itemCount);
    }
  }

  cr.addSingletonGetter(PluralStringProxyImpl);

  // #cr_define_end
  return {PluralStringProxy, PluralStringProxyImpl};
});
