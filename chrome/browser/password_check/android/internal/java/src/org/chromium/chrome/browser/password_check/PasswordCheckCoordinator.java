// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.content.Context;

import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;

/**
 * Creates the PasswordCheck component. This class is responsible for managing the check of the
 * leaked password through the internal components of the feature.
 */
public class PasswordCheckCoordinator implements PasswordCheckComponent {
    @Override
    public void initialize(Context context) {
        SettingsLauncher launcher = new SettingsLauncherImpl();
        launcher.launchSettingsActivity(context, PasswordCheckSettingsView.class);
    }
}