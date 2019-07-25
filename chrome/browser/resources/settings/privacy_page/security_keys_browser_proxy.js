// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * Credential represents a CTAP2 resident credential enumerated from a security
 * key.
 *
 * id: (required) The hex encoding of the CBOR-serialized
 *     PublicKeyCredentialDescriptor of the credential.
 *
 * relyingPartyId: (required) The RP ID (i.e. the site that created the
 *     credential; eTLD+n)
 *
 * userName: (required) The PublicKeyCredentialUserEntity.name
 *
 * userDisplayName: (required) The PublicKeyCredentialUserEntity.display_name
 *
 * @typedef {{id: string,
 *            relyingPartyId: string,
 *            userName: string,
 *            userDisplayName: string}}
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
let Credential;

cr.define('settings', function() {
  /** @interface */
  class SecurityKeysPINBrowserProxy {
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

    /** Cancels all outstanding operations. */
    close() {}
  }

  /** @interface */
  class SecurityKeysCredentialBrowserProxy {
    /**
     * Starts a credential management operation.
     *
     * Callers must listen to errors that can occur during the operation via a
     * 'security-keys-credential-management-error' WebListener. Values received
     * via this listener are localized error strings. When the
     * WebListener fires, the operation must be considered terminated.
     *
     * @return {!Promise} a promise that resolves when the handler is ready for
     *     the authenticator PIN to be provided.
     */
    startCredentialManagement() {}

    /**
     * Provides a PIN for a credential management operation. The
     * startCredentialManagement() promise must have resolved before this method
     * may be called.
     * @return {!Promise<?number>} a promise that resolves with null if the PIN
     *     was correct or the number of retries remaining otherwise.
     */
    providePIN(pin) {}

    /**
     * Enumerates credentials on the authenticator. A correct PIN must have
     * previously been supplied via providePIN() before this
     * method may be called.
     * @return {!Promise<!Array<!Credential>>}
     */
    enumerateCredentials() {}

    /**
     * Deletes the credentials with the given IDs from the security key.
     * @param {!Array<string>} ids
     * @return {!Promise<string>} a localized response message to display to
     *     the user (on either success or error)
     */
    deleteCredentials(ids) {}

    /** Cancels all outstanding operations. */
    close() {}
  }

  /** @interface */
  class SecurityKeysResetBrowserProxy {
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

    /** Cancels all outstanding operations. */
    close() {}
  }

  /** @implements {settings.SecurityKeysPINBrowserProxy} */
  class SecurityKeysPINBrowserProxyImpl {
    /** @override */
    startSetPIN() {
      return cr.sendWithPromise('securityKeyStartSetPIN');
    }

    /** @override */
    setPIN(oldPIN, newPIN) {
      return cr.sendWithPromise('securityKeySetPIN', oldPIN, newPIN);
    }

    /** @override */
    close() {
      return chrome.send('securityKeyPINClose');
    }
  }

  /** @implements {settings.SecurityKeysCredentialBrowserProxy} */
  class SecurityKeysCredentialBrowserProxyImpl {
    /** @override */
    startCredentialManagement() {
      return cr.sendWithPromise('securityKeyCredentialManagementStart');
    }

    /** @override */
    providePIN(pin) {
      return cr.sendWithPromise('securityKeyCredentialManagementPIN', pin);
    }

    /** @override */
    enumerateCredentials() {
      return cr.sendWithPromise('securityKeyCredentialManagementEnumerate');
    }

    /** @override */
    deleteCredentials(ids) {
      return cr.sendWithPromise('securityKeyCredentialManagementDelete', ids);
    }

    /** @override */
    close() {
      return chrome.send('securityKeyCredentialManagementClose');
    }
  }

  /** @implements {settings.SecurityKeysResetBrowserProxy} */
  class SecurityKeysResetBrowserProxyImpl {
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
      return chrome.send('securityKeyResetClose');
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(SecurityKeysPINBrowserProxyImpl);
  cr.addSingletonGetter(SecurityKeysCredentialBrowserProxyImpl);
  cr.addSingletonGetter(SecurityKeysResetBrowserProxyImpl);

  return {
    SecurityKeysPINBrowserProxy: SecurityKeysPINBrowserProxy,
    SecurityKeysPINBrowserProxyImpl: SecurityKeysPINBrowserProxyImpl,
    SecurityKeysCredentialBrowserProxy: SecurityKeysCredentialBrowserProxy,
    SecurityKeysCredentialBrowserProxyImpl:
        SecurityKeysCredentialBrowserProxyImpl,
    SecurityKeysResetBrowserProxy: SecurityKeysResetBrowserProxy,
    SecurityKeysResetBrowserProxyImpl: SecurityKeysResetBrowserProxyImpl,
  };
});
