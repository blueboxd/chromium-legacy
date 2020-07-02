// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.content.Context;

import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;

/**
 * This class is responsible for managing the saved passwords check for signed-in users.
 */
class PasswordCheckImpl implements PasswordCheck {
    @Override
    public void showUi(Context context) {
        SettingsLauncher launcher = new SettingsLauncherImpl();
        launcher.launchSettingsActivity(context, PasswordCheckFragmentView.class);
    }
}