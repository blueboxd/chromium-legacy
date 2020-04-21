// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceDialogFragmentCompat;

import org.chromium.base.Callback;
import org.chromium.chrome.R;

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
    protected void onBindDialogView(View view) {
        TextView signedOutView = view.findViewById(R.id.signed_out_text);
        TextView offlineTextView = view.findViewById(R.id.offline_text);
        signedOutView.setText(ClearWebsiteStorage.getSignedOutText());
        offlineTextView.setText(ClearWebsiteStorage.getOfflineText());

        super.onBindDialogView(view);
    }

    @Override
    public void onDialogClosed(boolean positiveResult) {
        sCallback.onResult(positiveResult);
    }
}
