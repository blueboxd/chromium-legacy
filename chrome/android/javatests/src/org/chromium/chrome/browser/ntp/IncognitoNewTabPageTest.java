// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.scrollTo;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isChecked;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.isNotChecked;
import static android.support.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;

import android.support.test.espresso.matcher.ViewMatchers;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.content_settings.ContentSettingsFeatureList;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Integration tests for IncognitoNewTabPage.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ContentSettingsFeatureList.IMPROVED_COOKIE_CONTROLS)
public class IncognitoNewTabPageTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void setThirdPartyCookieBlocking(boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge.getInstance().setBoolean(Pref.BLOCK_THIRD_PARTY_COOKIES, enabled);
        });
    }

    private void setCookieControlsMode(@CookieControlsMode int mode) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge.getInstance().setInteger(Pref.COOKIE_CONTROLS_MODE, mode);
        });
    }

    private void assertCookieControlsMode(@CookieControlsMode int mode) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    PrefServiceBridge.getInstance().getInteger(Pref.COOKIE_CONTROLS_MODE), mode);
        });
    }

    /**
     * Test cookie controls card is GONE when cookie controls flag disabled.
     */
    @Test
    @SmallTest
    @DisableFeatures(ContentSettingsFeatureList.IMPROVED_COOKIE_CONTROLS)
    public void testCookieControlsCardGONE() throws Exception {
        mActivityTestRule.newIncognitoTabFromMenu();
        onView(withId(R.id.cookie_controls_card))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
    }

    /**
     * Test cookie controls toggle defaults to on if cookie controls mode is on.
     */
    @Test
    @SmallTest
    public void testCookieControlsToggleStartsOn() throws Exception {
        setThirdPartyCookieBlocking(false);
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        mActivityTestRule.newIncognitoTabFromMenu();

        // Make sure cookie controls card is visible
        onView(withId(R.id.cookie_controls_card))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
        // Assert the cookie controls toggle is checked
        onView(withId(R.id.cookie_controls_card_toggle)).check(matches(isChecked()));
    }

    /**
     * Test cookie controls toggle turns on and off cookie controls mode as expected.
     */
    @Test
    @SmallTest
    public void testCookieControlsToggleChanges() throws Exception {
        setThirdPartyCookieBlocking(false);
        setCookieControlsMode(CookieControlsMode.OFF);
        mActivityTestRule.newIncognitoTabFromMenu();
        onView(withId(R.id.cookie_controls_card))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        int toggle_id = R.id.cookie_controls_card_toggle;
        // Toggle should start unchecked
        onView(withId(toggle_id)).check(matches(isNotChecked()));
        // Toggle should be checked after click
        onView(withId(toggle_id)).perform(scrollTo(), click()).check(matches(isChecked()));
        // CookieControlsMode should be incognito_only
        assertCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        // Toggle should be unchecked again after click
        onView(withId(toggle_id)).perform(scrollTo(), click()).check(matches(isNotChecked()));
        // CookieControlsMode should be off
        assertCookieControlsMode(CookieControlsMode.OFF);
    }

    /**
     * Test cookie controls disabled if managed by settings.
     */
    @Test
    @SmallTest
    public void testCookieControlsToggleManaged() throws Exception {
        setThirdPartyCookieBlocking(false);
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        mActivityTestRule.newIncognitoTabFromMenu();
        onView(withId(R.id.cookie_controls_card))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        int toggle_id = R.id.cookie_controls_card_toggle;
        // Toggle should start checked and enabled
        onView(withId(toggle_id)).check(matches(allOf(isChecked(), isEnabled())));
        // Toggle should be disabled if managed by setting
        setThirdPartyCookieBlocking(true);
        onView(withId(toggle_id)).check(matches(not(isEnabled())));
        // Toggle should be enabled and remain checked
        setThirdPartyCookieBlocking(false);
        onView(withId(toggle_id)).check(matches(allOf(isChecked(), isEnabled())));

        // Repeat of above but toggle should remain unchecked
        onView(withId(toggle_id)).perform(scrollTo(), click());
        onView(withId(toggle_id)).check(matches(allOf(isNotChecked(), isEnabled())));
        setThirdPartyCookieBlocking(true);
        onView(withId(toggle_id)).check(matches(not(isEnabled())));
        setThirdPartyCookieBlocking(false);
        onView(withId(toggle_id)).check(matches(allOf(isNotChecked(), isEnabled())));
    }
}
