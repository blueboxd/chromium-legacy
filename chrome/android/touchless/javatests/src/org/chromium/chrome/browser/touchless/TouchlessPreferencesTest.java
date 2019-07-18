// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesTest;
import org.chromium.chrome.browser.preferences.website.SingleCategoryPreferences;
import org.chromium.chrome.browser.preferences.website.SiteSettingsCategory;
import org.chromium.chrome.browser.preferences.website.SiteSettingsPreferences;
import org.chromium.chrome.browser.preferences.website.SiteSettingsTestUtils;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;

/**
 * This class tests touchless-specific modifications that were applied to preferences.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures(ChromeFeatureList.CAPTION_SETTINGS)
public class TouchlessPreferencesTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOptionsMenu() throws Exception {
        final Preferences activity =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SiteSettingsPreferences.class.getName());
        Assert.assertTrue(activity instanceof TouchlessPreferences);

        ViewGroup actionBarView = activity.findViewById(R.id.action_bar);
        Assert.assertEquals("Options menu should not have any items in touchless mode", 0,
                ((ViewGroup) actionBarView.getChildAt(1)).getChildCount());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testNoClipboardInSiteSettings() throws Exception {
        final Preferences activity =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SiteSettingsPreferences.class.getName());
        Assert.assertTrue(activity instanceof TouchlessPreferences);

        SiteSettingsPreferences preferences =
                (SiteSettingsPreferences) activity.getMainFragmentCompat();
        Assert.assertNull(preferences.findPreference(
                SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.CLIPBOARD)));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testNoThirdPartyCookiesInSiteSettings() throws Exception {
        final Preferences activity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        Assert.assertTrue(activity instanceof TouchlessPreferences);

        SingleCategoryPreferences cookiesPreferences =
                (SingleCategoryPreferences) activity.getMainFragmentCompat();
        Assert.assertNull(cookiesPreferences.findPreference(
                SingleCategoryPreferences.THIRD_PARTY_COOKIES_TOGGLE_KEY));
    }
}
