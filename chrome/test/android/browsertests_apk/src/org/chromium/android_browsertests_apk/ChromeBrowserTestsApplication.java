// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_browsertests_apk;

import android.content.Context;

import org.chromium.base.PathUtils;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.native_test.NativeBrowserTestApplication;

/**
 * A basic chrome.browser.tests {@link android.app.Application}.
 *
 * TODO(danakj): This class sets up some of the things that ChromeApplication
 * does but we should maybe subclass ChromeApplication or share code. There are
 * many things missing from attachBaseContext() that ChromeApplication sets up
 * and it's not clear yet if they are needed or not.
 */
public class ChromeBrowserTestsApplication extends NativeBrowserTestApplication {
    static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";

    private final GlobalDiscardableReferencePool mReferencePool =
            new GlobalDiscardableReferencePool();

    @Override
    protected void attachBaseContext(Context base) {
        boolean isBrowserProcess = isBrowserProcess();

        if (isBrowserProcess) UmaUtils.recordMainEntryPointTime();

        super.attachBaseContext(base);

        if (isBrowserProcess) {
            // Test-only stuff, see also NativeUnitTest.java.
            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        }
    }
}
