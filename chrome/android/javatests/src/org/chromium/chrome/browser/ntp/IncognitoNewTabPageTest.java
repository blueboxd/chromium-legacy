// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.support.test.filters.SmallTest;
import android.support.v7.widget.SwitchCompat;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Integration tests for IncognitoNewTabPage.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.IMPROVED_COOKIE_CONTROLS)
public class IncognitoNewTabPageTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Test cookie controls card is GONE when cookie controls flag disabled.
     */
    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.IMPROVED_COOKIE_CONTROLS)
    public void testCookieControlsCardGONE() throws Exception {
        mActivityTestRule.newIncognitoTabFromMenu();
        final IncognitoNewTabPage ntp = (IncognitoNewTabPage) mActivityTestRule.getActivity()
                                                .getActivityTab()
                                                .getNativePage();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    ntp.getView().findViewById(R.id.cookie_controls_card).getVisibility(),
                    View.GONE);
        });
    }

    /**
     * Test cookie controls toggle defaults to on if cookie controls mode is on.
     */
    @Test
    @SmallTest
    public void testCookieControlsToggleStartsOn() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Set normal third-party cookie blocking to off.
            PrefServiceBridge.getInstance().setBoolean(Pref.BLOCK_THIRD_PARTY_COOKIES, false);
            // Set CookieControlsMode Pref to On
            PrefServiceBridge.getInstance().setInteger(
                    Pref.COOKIE_CONTROLS_MODE, CookieControlsMode.INCOGNITO_ONLY);
        });

        mActivityTestRule.newIncognitoTabFromMenu();
        final IncognitoNewTabPage ntp = (IncognitoNewTabPage) mActivityTestRule.getActivity()
                                                .getActivityTab()
                                                .getNativePage();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Make sure cookie controls card is visible
            Assert.assertEquals(
                    ntp.getView().findViewById(R.id.cookie_controls_card).getVisibility(),
                    View.VISIBLE);
            // Assert the cookie controls toggle is checked
            Assert.assertTrue(
                    ((SwitchCompat) ntp.getView().findViewById(R.id.cookie_controls_card_toggle))
                            .isChecked());
        });
    }

    /**
     * Test cookie controls toggle turns on and off cookie controls mode as expected.
     */
    @Test
    @SmallTest
    public void testCookieControlsToggleChanges() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Set normal third-party cookie blocking to off.
            PrefServiceBridge.getInstance().setBoolean(Pref.BLOCK_THIRD_PARTY_COOKIES, false);
            // Set CookieControlsMode Pref to Off
            PrefServiceBridge.getInstance().setInteger(
                    Pref.COOKIE_CONTROLS_MODE, CookieControlsMode.OFF);
        });

        mActivityTestRule.newIncognitoTabFromMenu();
        final IncognitoNewTabPage ntp = (IncognitoNewTabPage) mActivityTestRule.getActivity()
                                                .getActivityTab()
                                                .getNativePage();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Make sure cookie controls card is visible
            Assert.assertEquals(
                    ntp.getView().findViewById(R.id.cookie_controls_card).getVisibility(),
                    View.VISIBLE);

            SwitchCompat toggle =
                    (SwitchCompat) ntp.getView().findViewById(R.id.cookie_controls_card_toggle);
            Assert.assertFalse("Toggle should be unchecked", toggle.isChecked());
            toggle.performClick();
            Assert.assertTrue("Toggle should be checked", toggle.isChecked());
            Assert.assertEquals("CookieControlsMode should be incognito_only",
                    PrefServiceBridge.getInstance().getInteger(Pref.COOKIE_CONTROLS_MODE),
                    CookieControlsMode.INCOGNITO_ONLY);
            toggle.performClick();
            Assert.assertFalse("Toggle should be unchecked again", toggle.isChecked());
            Assert.assertEquals("CookieControlsMode should be off",
                    PrefServiceBridge.getInstance().getInteger(Pref.COOKIE_CONTROLS_MODE),
                    CookieControlsMode.OFF);
        });
    }
}
