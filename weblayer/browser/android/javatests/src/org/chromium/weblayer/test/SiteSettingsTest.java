// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.weblayer.SiteSettingsActivity;

/**
 * Tests the behavior of the Site Settings UI.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class SiteSettingsTest {
    private static final String PROFILE_NAME = "DefaultProfile";

    @Rule
    public SiteSettingsActivityTestRule mActivityTestRule = new SiteSettingsActivityTestRule();

    @Test
    @SmallTest
    @MinWebLayerVersion(84)
    public void testSiteSettingsLaunches() throws InterruptedException {
        SiteSettingsActivity siteSettingsActivity =
                mActivityTestRule.launchCategoryListWithProfile(PROFILE_NAME);

        // Check that the "All sites" option is visible.
        onView(withText("All sites")).check(matches(isDisplayed()));
    }
}
