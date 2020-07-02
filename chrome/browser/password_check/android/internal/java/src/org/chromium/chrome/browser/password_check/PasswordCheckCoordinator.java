// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.view.MenuItem;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Creates the PasswordCheckComponentUi. This class is responsible for managing the UI for the check
 * of the leaked password.
 */
class PasswordCheckCoordinator implements PasswordCheckComponentUi {
    private final PasswordCheckFragmentView mFragmentView;

    PasswordCheckCoordinator(PasswordCheckFragmentView fragmentView) {
        mFragmentView = fragmentView;
    }

    // TODO(crbug.com/1101256): Move to view code.
    @Override
    public boolean handleHelp(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedback.getInstance().show(mFragmentView.getActivity(),
                    mFragmentView.getActivity().getString(R.string.help_context_check_passwords),
                    Profile.getLastUsedRegularProfile(), null);
            return true;
        }
        return false;
    }
}