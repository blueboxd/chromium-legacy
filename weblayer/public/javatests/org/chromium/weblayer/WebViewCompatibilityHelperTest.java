// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.Pair;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.MinAndroidSdkLevel;

/**
 * Tests for (@link WebViewCompatibilityHelper}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebViewCompatibilityHelperTest {
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    public void testLibraryPaths() throws Exception {
        Context appContext = InstrumentationRegistry.getTargetContext();
        Context remoteContext = WebLayer.getOrCreateRemoteContext(appContext);
        Pair<ClassLoader, WebLayer.WebViewCompatibilityResult> result =
                WebViewCompatibilityHelper.initialize(appContext, remoteContext);
        Assert.assertEquals(result.second, WebLayer.WebViewCompatibilityResult.SUCCESS);
        String[] libraryPaths = WebViewCompatibilityHelper.getLibraryPaths(result.first);
        for (String path : libraryPaths) {
            Assert.assertTrue(path.startsWith("/./"));
        }
    }

    @Test
    @SmallTest
    public void testSupportedVersion() throws Exception {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            Assert.assertFalse(WebViewCompatibilityHelper.isSupportedVersion("81.0.2.5"));
        } else {
        Assert.assertTrue(WebViewCompatibilityHelper.isSupportedVersion("81.0.2.5"));
        }
        Assert.assertTrue(WebViewCompatibilityHelper.isSupportedVersion("82.0.2.5"));
        Assert.assertFalse(WebViewCompatibilityHelper.isSupportedVersion("80.0.2.5"));
        Assert.assertFalse(WebViewCompatibilityHelper.isSupportedVersion(""));
        Assert.assertFalse(WebViewCompatibilityHelper.isSupportedVersion("82.0"));
        Assert.assertFalse(WebViewCompatibilityHelper.isSupportedVersion(null));
    }
}
