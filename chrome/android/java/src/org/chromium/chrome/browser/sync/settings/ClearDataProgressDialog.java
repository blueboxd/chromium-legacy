// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.app.Dialog;
import android.app.ProgressDialog;
import android.os.Bundle;

import androidx.fragment.app.DialogFragment;

/**
 * This class is used by {@link AccountManagementFragment} to show the clear data progress dialog
 * during sign out.
 */
public class ClearDataProgressDialog extends DialogFragment {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Don't allow the dialog to be recreated by Android, since it wouldn't ever be
        // dismissed after recreation.
        if (savedInstanceState != null) dismiss();
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        setCancelable(false);
        ProgressDialog dialog = new ProgressDialog(getActivity());
        dialog.setTitle(getString(org.chromium.chrome.R.string.wiping_profile_data_title));
        dialog.setMessage(getString(org.chromium.chrome.R.string.wiping_profile_data_message));
        dialog.setIndeterminate(true);
        return dialog;
    }
}
