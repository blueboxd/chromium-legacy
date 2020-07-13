// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.content.Intent;
import android.net.Uri;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.ui.base.WindowAndroid;

/**
 * A utitily class for launching the password leak check.
 */
public class PasswordCheckupLauncher {
    @CalledByNative
    private static void launchCheckupInAccount(String checkupUrl, WindowAndroid windowAndroid) {
        if (windowAndroid.getContext().get() == null) return; // Window not available yet/anymore.
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        if (tryLaunchingNativePasswordCheckup(activity)) return;
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(checkupUrl));
        intent.setPackage(activity.getPackageName());
        activity.startActivity(intent);
    }

    @CalledByNative
    private static void launchLocalCheckup(WindowAndroid windowAndroid) {
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_CHECK);
        if (windowAndroid.getContext().get() == null) return; // Window not available yet/anymore.
        PasswordCheckFactory.create().showUi(windowAndroid.getContext().get());
    }

    private static boolean tryLaunchingNativePasswordCheckup(ChromeActivity activity) {
        GooglePasswordManagerUIProvider googlePasswordManagerUIProvider =
                AppHooks.get().createGooglePasswordManagerUIProvider();
        if (googlePasswordManagerUIProvider == null) return false;
        return googlePasswordManagerUIProvider.launchPasswordCheckup(activity);
    }
}
