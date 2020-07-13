// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import org.chromium.base.annotations.NativeMethods;

/**
 * Class handling the communication with the C++ part of the password check feature. It forwards
 * messages to and from its C++ counterpart.
 */
class PasswordCheckBridge {
    private final long mNativePasswordCheckBridge;
    private final PasswordCheckObserver mPasswordCheckObserver;

    /**
     * Observer listening to all messages relevant to the password check.
     */
    interface PasswordCheckObserver {
        /**
         * Called when a new compromised credential is found by the password check
         * @param originUrl Origin of the compromised credential.
         * @param username Username for the compromised credential.
         * @param password Password of the compromised credential.
         */
        void onCompromisedCredentialFound(String originUrl, String username, String password);

        /**
         * Called when the compromised credentials found in a previous check are read from disk.
         * @param count The number of compromised credentials that were found in a previous check.
         */
        void onCompromisedCredentialsFetched(int count);
    }

    PasswordCheckBridge(PasswordCheckObserver passwordCheckObserver) {
        // Initialized its native counterpart. This will also start fetching the compromised
        // credentials stored in the database by the last check.
        mNativePasswordCheckBridge = PasswordCheckBridgeJni.get().create();
        mPasswordCheckObserver = passwordCheckObserver;
    }

    // TODO(crbug.com/1102025): Add call from native.
    void onCompromisedCredentialFound(String originUrl, String username, String password) {
        mPasswordCheckObserver.onCompromisedCredentialFound(originUrl, username, password);
    }

    // TODO(crbug.com/1102025): Add call from native.
    void onCompromisedCredentialsFetched(int count) {
        mPasswordCheckObserver.onCompromisedCredentialsFetched(count);
    }

    // TODO(crbug.com/1102025): Add call from native.
    private static void insertCredential(CompromisedCredential[] credentials, int index,
            String originUrl, String username, String password, boolean phished) {
        credentials[index] = new CompromisedCredential(originUrl, username, password, phished);
    }

    /**
     * Starts the password check.
     */
    void startCheck() {
        PasswordCheckBridgeJni.get().startCheck(mNativePasswordCheckBridge);
    }

    /**
     * Stops the password check.
     */
    void stopCheck() {
        PasswordCheckBridgeJni.get().stopCheck(mNativePasswordCheckBridge);
    }

    /**
     * This can return 0 if the compromised credentials haven't been fetched from the database yet.
     * @return The number of compromised credentials found in the last run password check.
     */
    int getCompromisedCredentialsCount() {
        return PasswordCheckBridgeJni.get().getCompromisedCredentialsCount(
                mNativePasswordCheckBridge);
    }

    /**
     * Returns the list of compromised credentials that are stored in the database.
     * @param credentials array to be populated with the compromised credentials.
     */
    void getCompromisedCredentials(CompromisedCredential[] credentials) {
        PasswordCheckBridgeJni.get().getCompromisedCredentials(
                mNativePasswordCheckBridge, credentials);
    }

    /**
     * C++ method signatures.
     */
    @NativeMethods
    interface Natives {
        long create();
        void startCheck(long nativePasswordCheckBridge);
        void stopCheck(long nativePasswordCheckBridge);
        int getCompromisedCredentialsCount(long nativePasswordCheckBridge);
        void getCompromisedCredentials(
                long nativePasswordCheckBridge, CompromisedCredential[] credentials);
        void destroy(long nativePasswordCheckBridge);
    }
}
