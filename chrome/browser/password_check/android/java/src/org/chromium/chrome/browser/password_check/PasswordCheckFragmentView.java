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

    /**
     * Set the delegate that handles view events which affect the state of the component.
     * @param componentDelegate The {@link PasswordCheckComponentUi} delegate.
     */
    void setComponentDelegate(PasswordCheckComponentUi componentDelegate) {
        mComponentDelegate = componentDelegate;
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.passwords_check_title);
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getStyledContext()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        return mComponentDelegate.handleHelp(item);
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

}