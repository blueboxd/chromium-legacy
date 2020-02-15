// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.variations;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;

/**
 * Stores values related to the collection of variations service metrics.
 *
 * The values maintained by this class can be initialized from and serialized to a dedicated
 * variations SharedPreferences, or from a Bundle suitable for sending in AIDL IPC calls.
 */
public class VariationsServiceMetricsHelper {
    private static final String PREF_FILE_NAME = "variations_prefs";

    // The duration in milliseconds of the last seed download.
    private static final String SEED_FETCH_TIME = "seed_fetch_time";

    /**
     * Creates a new VariationsServiceMetricsHelper instance initialized with the contents of the
     * given Bundle.
     */
    public static VariationsServiceMetricsHelper fromBundle(Bundle bundle) {
        return new VariationsServiceMetricsHelper(bundle);
    }

    /**
     * Creates a new VariationsServiceMetricsHelper instance initialized with the contents of the
     * variations SharedPreferences instance for the given Context.
     */
    public static VariationsServiceMetricsHelper fromVariationsSharedPreferences(Context context) {
        Bundle bundle = new Bundle();
        SharedPreferences prefs =
                context.getSharedPreferences(PREF_FILE_NAME, Context.MODE_PRIVATE);
        if (prefs.contains(SEED_FETCH_TIME)) {
            bundle.putLong(SEED_FETCH_TIME, prefs.getLong(SEED_FETCH_TIME, 0));
        }
        return new VariationsServiceMetricsHelper(bundle);
    }

    private final Bundle mBundle;

    public Bundle toBundle() {
        // Create a copy of the Bundle to make sure it won't be modified by future mutator method
        // calls in this class.
        return new Bundle(mBundle);
    }

    // This method should only be called from within WebView's service.
    public boolean writeMetricsToVariationsSharedPreferences(Context context) {
        SharedPreferences.Editor prefsEditor =
                context.getSharedPreferences(PREF_FILE_NAME, Context.MODE_PRIVATE).edit();
        prefsEditor.clear();
        if (mBundle.containsKey(SEED_FETCH_TIME)) {
            prefsEditor.putLong(SEED_FETCH_TIME, mBundle.getLong(SEED_FETCH_TIME));
        }
        return prefsEditor.commit();
    }

    public void clearSeedFetchTime() {
        mBundle.remove(SEED_FETCH_TIME);
    }
    public void setSeedFetchTime(long seedFetchTime) {
        mBundle.putLong(SEED_FETCH_TIME, seedFetchTime);
    }
    public boolean hasSeedFetchTime() {
        return mBundle.containsKey(SEED_FETCH_TIME);
    }
    public long getSeedFetchTime() {
        return mBundle.getLong(SEED_FETCH_TIME);
    }

    private VariationsServiceMetricsHelper(Bundle bundle) {
        mBundle = bundle;
    }
}
