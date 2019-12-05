// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.content.Intent;

import androidx.annotation.Nullable;

/**
 * Client used to communicate with GmsCore about sync encryption keys.
 */
public class TrustedVaultClient {
    /**
     * Displays a UI that allows the user to reauthenticate and retrieve the sync encryption keys.
     */
    public static void displayKeyRetrievalDialog() {
        // TODO(crbug.com/1012659): Not implemented.
    }

    /**
     * Creates an intent that launches an activity that triggers the key retrieval UI.
     *
     * @return the intent for opening the key retrieval activity or null if none is actually
     * required
     */
    @Nullable
    public static Intent createKeyRetrievalIntent() {
        // TODO(crbug.com/1012659): Not implemented.
        return null;
    }
}
