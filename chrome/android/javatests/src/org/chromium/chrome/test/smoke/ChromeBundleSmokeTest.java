// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.smoke;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;

/** Smoke Test for Chrome bundles. */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class ChromeBundleSmokeTest {
    private static final String TARGET_ACTIVITY =
            "org.chromium.chrome.features.test_dummy.TestDummyActivity";

    public ChromeUiAutomatorTestRule mRule = new ChromeUiAutomatorTestRule();
    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();
    @Rule
    public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mRule);

    private String mPackageName;

    @Before
    public void setUp() {
        mPackageName = InstrumentationRegistry.getArguments().getString(
                ChromeUiApplicationTestRule.PACKAGE_NAME_ARG);
        Assert.assertNotNull("Must specify bundle under test", mPackageName);
        mChromeUiRule.launchIntoNewTabPageOnFirstRun();
    }

    @Test
    public void testModuleInstall() {
        // Send intent that makes Chrome install the test dummy module.
        Context context = InstrumentationRegistry.getContext();
        Intent intent = new Intent();
        intent.setComponent(new ComponentName(mPackageName, TARGET_ACTIVITY));
        intent.putExtra("test_case", 0); // Test case EXECUTE_JAVA.
        intent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);

        // Wait for done dialog to show up.
        Assert.assertTrue(UiAutomatorUtils.getInstance().getLocatorHelper().isOnScreen(
                Ui2Locators.withText("Test Case 0: done")));
    }
}
