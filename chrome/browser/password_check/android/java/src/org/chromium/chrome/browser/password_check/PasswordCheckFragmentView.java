// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.content.Context;
import android.os.Bundle;
import android.view.MenuItem;

import androidx.preference.PreferenceFragmentCompat;

/**
 * This class is responsible for rendering the check passwords view in the settings menu.
 */
public class PasswordCheckFragmentView extends PreferenceFragmentCompat {
    private PasswordCheckComponentUi mComponentDelegate;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.passwords_check_title);
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getStyledContext()));
        mComponentDelegate = PasswordCheckComponentUiFactory.create(this);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        return mComponentDelegate.handleHelp(item);
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }
}