// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

/**
 * Fragment containing Safety check.
 */
public class SafetyCheckSettingsFragment extends PreferenceFragmentCompat {
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        getActivity().setTitle(R.string.prefs_safety_check);
    }
}
