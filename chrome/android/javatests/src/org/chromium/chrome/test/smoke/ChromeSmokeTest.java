// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.smoke;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.uiautomator.UiDevice;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

/**
 * Smoke Test for Chrome Android.
 */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class ChromeSmokeTest {
    private static final String PACKAGE_NAME_ARG = "PackageUnderTest";
    private static final String DATA_URL = "data:,Hello";
    private static final String ACTIVITY_NAME = "org.chromium.chrome.browser.ChromeTabbedActivity";

    public final long mTimeout = ScalableTimeout.scaleTimeout(5000L);
    public static final long UI_CHECK_INTERVAL = 200L;
    private String mPackageName;

    @Before
    public void setUp() {
        // TODO (aluo): Adjust this as needed according to cl 1585142
        mPackageName = InstrumentationRegistry.getArguments().getString(PACKAGE_NAME_ARG);
        if (mPackageName == null) {
            mPackageName = InstrumentationRegistry.getTargetContext().getPackageName();
        }
    }

    @Test
    public void testHello() {
        Context context = InstrumentationRegistry.getContext();
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(DATA_URL));
        intent.setComponent(new ComponentName(mPackageName, ACTIVITY_NAME));
        context.startActivity(intent);

        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        // TODO (aluo): Check that the data url is loaded after pagecontroller lands.
        IUi2Locator locatorChrome = Ui2Locators.withPackageName(mPackageName);

        CriteriaHelper.pollInstrumentationThread(() -> {
            return locatorChrome.locateOne(device) != null;
        }, mPackageName + " should have loaded", mTimeout, UI_CHECK_INTERVAL);
    }
}
