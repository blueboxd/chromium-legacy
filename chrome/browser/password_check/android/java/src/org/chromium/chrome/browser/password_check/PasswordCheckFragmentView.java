// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.content.Context;
import android.os.Bundle;
import android.view.MenuItem;

import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceFragmentCompat;

/**
 * This class is responsible for rendering the check passwords view in the settings menu.
 */
public class PasswordCheckFragmentView extends PreferenceFragmentCompat {
    private PasswordCheckComponentUi mComponentDelegate;

    /**
     * The factory used to create components that connect to this fragment and provide data. It
     * defaults to the {@link PasswordCheckComponentUiFactory} but can be replaced in tests.
     */
    interface ComponentFactory {
        /**
         * Returns a component that is connected to the given fragment and manipulates its data.
         * @param fragmentView The fragment (usually {@code this}).
         * @return A non-null {@link PasswordCheckComponentUi}.
         */
        PasswordCheckComponentUi create(PreferenceFragmentCompat fragmentView);
    }
    private static ComponentFactory sComponentFactory = PasswordCheckComponentUiFactory::create;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.passwords_check_title);
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getStyledContext()));
        mComponentDelegate = sComponentFactory.create(this);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        return mComponentDelegate.handleHelp(item);
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    @VisibleForTesting
    static void setComponentFactory(ComponentFactory factory) {
        sComponentFactory = factory;
    }
}