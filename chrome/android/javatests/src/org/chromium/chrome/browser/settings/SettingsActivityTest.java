// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import android.support.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.policy.test.annotations.Policies;

/**
 * Tests for the Settings menu.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SettingsActivityTest {
    @Rule
    public SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    @Test
    @SmallTest
    @Policies.Add({ @Policies.Item(key = "PasswordManagerEnabled", string = "false") })
    public void testPasswordSettings_ManagedAndDisabled() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeBrowserInitializer.getInstance().handleSynchronousStartup(); });

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return PrefServiceBridge.getInstance().isManagedPreference(
                        Pref.REMEMBER_PASSWORDS_ENABLED);
            }
        });

        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.password_settings_title)).perform(click());
        onView(withText(R.string.password_settings_save_passwords)).check(matches(isDisplayed()));
    }
}
