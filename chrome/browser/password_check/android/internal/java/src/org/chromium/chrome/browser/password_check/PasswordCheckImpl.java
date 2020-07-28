// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.content.Context;
import android.os.Bundle;

import org.chromium.chrome.browser.password_check.PasswordCheckBridge.PasswordCheckObserver;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;

/**
 * This class is responsible for managing the saved passwords check for signed-in users.
 */
class PasswordCheckImpl implements PasswordCheck, PasswordCheckObserver {
    private final PasswordCheckBridge mPasswordCheckBridge;

    PasswordCheckImpl() {
        mPasswordCheckBridge = new PasswordCheckBridge(this);
    }

    @Override
    public void showUi(Context context, @PasswordCheckReferrer int passwordCheckReferrer) {
        SettingsLauncher launcher = new SettingsLauncherImpl();
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(
                PasswordCheckFragmentView.PASSWORD_CHECK_REFERRER, passwordCheckReferrer);
        launcher.launchSettingsActivity(context, PasswordCheckFragmentView.class, fragmentArgs);
    }

    @Override
    public void destroy() {
        mPasswordCheckBridge.destroy();
    }

    @Override
    public void onCompromisedCredentialFound(String originUrl, String username, String password) {
        // TODO(crbug.com): Broadcast to registered observers.
    }

    @Override
    public void onCompromisedCredentialsFetched(int count) {
        // TODO(crbug.com): Broadcast to registered observers.
    }

    @Override
    public void removeCredential(CompromisedCredential credential) {
        // TODO(crbug.com/1106726): Call native method through bridge.
    }
}