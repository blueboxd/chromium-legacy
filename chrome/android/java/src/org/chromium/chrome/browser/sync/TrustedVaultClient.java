// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Promise;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.util.IntentUtils;

import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

/**
 * Client used to communicate with GmsCore about sync encryption keys.
 */
public class TrustedVaultClient {
    /**
     * Interface to downstream functionality.
     */
    public interface Backend {
        /**
         * Reads and returns available encryption keys without involving any user action.
         *
         * @param gaiaId String representation of the Gaia ID.
         * @return a promise with known keys, if any, where the last one is the most recent.
         */
        Promise<List<byte[]>> fetchKeys(String gaiaId);

        /**
         * Gets an Intent that can be used to display a UI that allows the user to reauthenticate
         * and retrieve the sync encryption keys.
         *
         * @return the Intent object or null is something went wrong.
         */
        @Nullable
        Intent createKeyRetrievalIntent();
    }

    /**
     * Trivial backend implementation that is always empty.
     */
    public static class EmptyBackend implements Backend {
        @Override
        public Promise<List<byte[]>> fetchKeys(String gaiaId) {
            return Promise.fulfilled(Collections.emptyList());
        }

        @Override
        public Intent createKeyRetrievalIntent() {
            return null;
        }
    };

    private static TrustedVaultClient sInstance;

    private final Backend mBackend;

    // Registered native TrustedVaultClientAndroid instances. Usually exactly one.
    private final Set<Long> mNativeTrustedVaultClientAndroidSet = new TreeSet<Long>();

    @VisibleForTesting
    public TrustedVaultClient(Backend backend) {
        assert backend != null;
        mBackend = backend;
    }

    /**
     * Displays a UI that allows the user to reauthenticate and retrieve the sync encryption keys.
     */
    public static TrustedVaultClient get() {
        if (sInstance == null) {
            sInstance =
                    new TrustedVaultClient(AppHooks.get().createSyncTrustedVaultClientBackend());
        }
        return sInstance;
    }

    /**
     * Displays a UI that allows the user to reauthenticate and retrieve the sync encryption keys.
     */
    public void displayKeyRetrievalDialog(Context context) {
        // TODO(crbug.com/1012659): Before starting the intent, one last attempt
        // should be made to read the currently-available keys.

        Intent intent = createKeyRetrievalIntent();
        if (intent == null) return;

        IntentUtils.safeStartActivity(context, intent);

        // TODO(crbug.com/1012659): Upon intent completion, the new keys should
        // fetched.
    }

    /**
     * Creates an intent that launches an activity that triggers the key retrieval UI.
     *
     * @return the intent for opening the key retrieval activity or null if none is actually
     * required
     */
    @Nullable
    public Intent createKeyRetrievalIntent() {
        return mBackend.createKeyRetrievalIntent();
    }

    /**
     * Registers a C++ client, which is a prerequisite before interacting with Java.
     */
    @CalledByNative
    private static void registerNative(long nativeTrustedVaultClientAndroid) {
        assert !isNativeRegistered(nativeTrustedVaultClientAndroid);
        get().mNativeTrustedVaultClientAndroidSet.add(nativeTrustedVaultClientAndroid);
    }

    /**
     * Unregisters a previously-registered client, canceling any in-flight requests.
     */
    @CalledByNative
    private static void unregisterNative(long nativeTrustedVaultClientAndroid) {
        assert isNativeRegistered(nativeTrustedVaultClientAndroid);
        get().mNativeTrustedVaultClientAndroidSet.remove(nativeTrustedVaultClientAndroid);
    }

    /**
     * Convenience function to check if a native client has been registered.
     */
    private static boolean isNativeRegistered(long nativeTrustedVaultClientAndroid) {
        return get().mNativeTrustedVaultClientAndroidSet.contains(nativeTrustedVaultClientAndroid);
    }

    /**
     * Forwards calls to Backend.fetchKeys() and upon completion invokes native method
     * fetchKeysCompleted().
     */
    @CalledByNative
    private static void fetchKeys(long nativeTrustedVaultClientAndroid, String gaiaId) {
        assert isNativeRegistered(nativeTrustedVaultClientAndroid);
        get().mBackend.fetchKeys(gaiaId).then(
                (keys)
                        -> {
                    if (isNativeRegistered(nativeTrustedVaultClientAndroid)) {
                        TrustedVaultClientJni.get().fetchKeysCompleted(
                                nativeTrustedVaultClientAndroid, gaiaId,
                                keys.toArray(new byte[0][]));
                    }
                },
                (exception) -> {
                    if (isNativeRegistered(nativeTrustedVaultClientAndroid)) {
                        TrustedVaultClientJni.get().fetchKeysCompleted(
                                nativeTrustedVaultClientAndroid, gaiaId, new byte[0][]);
                    }
                });
    }

    @NativeMethods
    interface Natives {
        void fetchKeysCompleted(long nativeTrustedVaultClientAndroid, String gaiaId, byte[][] keys);
    }
}
