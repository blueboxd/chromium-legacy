// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

/** JUnit test for WebappLauncherActivity. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebappLauncherActivityTest {
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";
    private static final String START_URL = "https://www.google.com/scope/a_is_for_apple";

    /**
     * Test that WebappLauncherActivity modifies the passed-in intent so that
     * WebApkIntentDataProviderFactory#create() returns null if the intent does not refer to a valid
     * WebAPK.
     */
    @Test
    public void tryCreateWebappInfoAltersIntentIfNotValidWebApk() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, null /* shareTargetMetaData */);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

        Assert.assertNotNull(WebApkIntentDataProviderFactory.create(intent));
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();
        Assert.assertNull(WebApkIntentDataProviderFactory.create(intent));
    }
}
