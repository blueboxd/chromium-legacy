// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.os.Bundle;

import androidx.preference.Preference;
import androidx.preference.PreferenceDialogFragmentCompat;

import org.chromium.base.Callback;

/**
 * The fragment used to display the clear website storage confirmation dialog.
 */
public class ClearWebsiteStorageDialog extends PreferenceDialogFragmentCompat {
    public static final String TAG = "ClearWebsiteStorageDialog";

    private static Callback<Boolean> sCallback;

    public static ClearWebsiteStorageDialog newInstance(
            Preference preference, Callback<Boolean> callback) {
        ClearWebsiteStorageDialog fragment = new ClearWebsiteStorageDialog();
        sCallback = callback;
        Bundle bundle = new Bundle(1);
        bundle.putString(PreferenceDialogFragmentCompat.ARG_KEY, preference.getKey());
        fragment.setArguments(bundle);
        return fragment;
    }

    @Override
    public void onDialogClosed(boolean positiveResult) {
        sCallback.onResult(positiveResult);
    }
}
