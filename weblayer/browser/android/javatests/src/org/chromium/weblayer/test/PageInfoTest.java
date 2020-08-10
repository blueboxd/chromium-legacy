// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.StrictModeContext;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.CriteriaNotSatisfiedException;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests the behavior of the Page Info UI.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class PageInfoTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    @MinWebLayerVersion(84)
    public void testPageInfoLaunches() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(
                mActivityTestRule.getTestDataURL("simple_page.html"));

        Context remoteContext = TestWebLayer.getRemoteContext(activity.getApplicationContext());
        int buttonId = ResourceUtil.getIdentifier(remoteContext, "id/security_button");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            StrictModeContext ignored = StrictModeContext.allowDiskReads();
            EventUtils.simulateTouchCenterOfView(activity.findViewById(buttonId));
        });
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                onView(withText("Your connection to this site is not secure"))
                        .check(matches(isDisplayed()));
            } catch (Error e) {
                throw new CriteriaNotSatisfiedException(e.toString());
            }
        });
    }
}
