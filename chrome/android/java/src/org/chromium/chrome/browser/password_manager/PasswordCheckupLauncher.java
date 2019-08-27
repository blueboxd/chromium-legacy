// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.content.Intent;
import android.net.Uri;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.base.WindowAndroid;

/**
 * A utitily class for launching the password leak check.
 */
public class PasswordCheckupLauncher {
    @CalledByNative
    private static void launchCheckup(String checkupUrl, WindowAndroid windowAndroid) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(checkupUrl));
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        intent.setPackage(activity.getPackageName());
        activity.startActivity(intent);
    }
}
