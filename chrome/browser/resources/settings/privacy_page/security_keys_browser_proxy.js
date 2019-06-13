// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

cr.define('settings', function() {
  /** @interface */
  class SecurityKeysBrowserProxy {
    /**
     * Starts a PIN set/change operation by flashing all security keys. Resolves
     * with a pair of numbers. The first is one if the process has immediately
     * completed (i.e. failed). In this case the second is a CTAP error code.
     * Otherwise the process is ongoing and must be completed by calling
     * |setPIN|. In this case the second number is either the number of tries
     * remaining to correctly specify the current PIN, or else null to indicate
     * that no PIN is currently set.
     * @return {!Promise<!Array<number>>}
     */
    startSetPIN() {}

    /**
     * Attempts a PIN set/change operation. Resolves with a pair of numbers
     * whose meaning is the same as with |startSetPIN|. The first number will
     * always be 1 to indicate that the process has completed and thus the
     * second will be the CTAP error code.
     * @return {!Promise<!Array<number>>}
     */
    setPIN(oldPIN, newPIN) {}

    /**
     * Starts a reset operation by flashing all security keys and sending a
     * reset command to the one that the user activates. Resolves with a CTAP
     * error code.
     * @return {!Promise<number>}
     */
    reset() {}

    /**
     * Waits for a reset operation to complete. Resolves with a CTAP error code.
     * @return {!Promise<number>}
     */
    completeReset() {}

    /**
     * Cancel all outstanding operations.
     */
    close() {}
  }

  /**
   * @implements {settings.SecurityKeysBrowserProxy}
   */
  class SecurityKeysBrowserProxyImpl {
    /** @override */
    startSetPIN() {
      return cr.sendWithPromise('securityKeyStartSetPIN');
    }

    /** @override */
    setPIN(oldPIN, newPIN) {
      return cr.sendWithPromise('securityKeySetPIN', oldPIN, newPIN);
    }

    /** @override */
    reset() {
      return cr.sendWithPromise('securityKeyReset');
    }

    /** @override */
    completeReset() {
      return cr.sendWithPromise('securityKeyCompleteReset');
    }

    /** @override */
    close() {
      return chrome.send('securityKeyClose');
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(SecurityKeysBrowserProxyImpl);

  return {
    SecurityKeysBrowserProxy: SecurityKeysBrowserProxy,
    SecurityKeysBrowserProxyImpl: SecurityKeysBrowserProxyImpl,
  };
});
