// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit test for {@link DefaultBrowserPromoUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DefaultBrowserPromoUtilsTest {
    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    // TODO(crbug.com/1090103): Add test for No Default case and other helper methods in
    // DefaultBrowserPromoUtils.
    // ResolveInfo#match is changed when intent is resolved, even if we mock it to 0 here.

    @Test
    public void testGetCurrentDefaultStateForOtherDefault() {
        ResolveInfo resolveInfo = new ResolveInfo();
        ActivityInfo activityInfo = new ActivityInfo();
        resolveInfo.match = 1;
        activityInfo.packageName = "android";
        resolveInfo.activityInfo = activityInfo;
        ShadowPackageManager packageManager =
                Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        packageManager.addResolveInfoForIntent(
                PackageManagerUtils.getQueryInstalledBrowsersIntent(), resolveInfo);
        Assert.assertEquals("Should be other default when resolve info matches another browser.",
                DefaultBrowserPromoUtils.DefaultBrowserState.OTHER_DEFAULT,
                DefaultBrowserPromoUtils.getCurrentDefaultBrowserState());
    }

    @Test
    public void testGetCurrentDefaultStateForChromeDefault() {
        ResolveInfo resolveInfo = new ResolveInfo();
        ActivityInfo activityInfo = new ActivityInfo();
        resolveInfo.match = 1;
        activityInfo.packageName = ContextUtils.getApplicationContext().getPackageName();
        resolveInfo.activityInfo = activityInfo;
        ShadowPackageManager packageManager =
                Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        packageManager.addResolveInfoForIntent(
                PackageManagerUtils.getQueryInstalledBrowsersIntent(), resolveInfo);
        Assert.assertEquals(
                "Should be chrome default when resolve info matches current package name.",
                DefaultBrowserPromoUtils.DefaultBrowserState.CHROME_DEFAULT,
                DefaultBrowserPromoUtils.getCurrentDefaultBrowserState());
    }
}
