// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.util.IntentUtils;

import java.util.Collections;
import java.util.List;

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
         * @return the list of known keys, if any, where the last one is the most recent.
         */
        List<byte[]> fetchKeys(String gaiaId);

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
        public List<byte[]> fetchKeys(String gaiaId) {
            return Collections.emptyList();
        }

        @Override
        public Intent createKeyRetrievalIntent() {
            return null;
        }
    };

    private static TrustedVaultClient sInstance;

    private final Backend mBackend;

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
     * Forwards calls to Backend.fetchKeys().
     */
    @CalledByNative
    private static byte[][] fetchKeys(String gaiaId) {
        List<byte[]> keys = get().mBackend.fetchKeys(gaiaId);
        return keys.toArray(new byte[0][]);
    }
}
