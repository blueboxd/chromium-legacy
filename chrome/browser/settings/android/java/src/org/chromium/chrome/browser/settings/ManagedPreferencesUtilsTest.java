// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;

import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;

import androidx.preference.Preference;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.settings.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests of {@link ManagedPreferencesUtils}.
 *
 * TODO(chouinard): Once SettingsLauncher and SettingsActivity have
 * compontentized interfaces, these test should be moved to
 * //components/browser_ui/settings/.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ManagedPreferencesUtilsTest {
    @Rule
    public ActivityTestRule<SettingsActivity> mRule =
            new ActivityTestRule<>(SettingsActivity.class);

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        Intent intent = SettingsLauncher.getInstance().createIntentForSettingsPage(
                context, DummySettingsForTest.class.getName());
        mRule.launchActivity(intent);
    }

    @Test
    @SmallTest
    public void testShowManagedByAdministratorToast() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferencesUtils.showManagedByAdministratorToast(mRule.getActivity());
        });

        onView(withText(R.string.managed_by_your_organization))
                .inRoot(withDecorView(not(mRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testShowManagedByParentToastNullDelegate() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferencesUtils.showManagedByParentToast(mRule.getActivity(), null);
        });

        onView(withText(R.string.managed_by_your_parent))
                .inRoot(withDecorView(not(mRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testShowManagedByParentToastSingleCustodian() {
        ManagedPreferenceDelegate singleCustodianDelegate = new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return false;
            }

            @Override
            public boolean isPreferenceControlledByCustodian(Preference preference) {
                return true;
            }

            @Override
            public boolean doesProfileHaveMultipleCustodians() {
                return false;
            }
        };

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferencesUtils.showManagedByParentToast(
                    mRule.getActivity(), singleCustodianDelegate);
        });

        onView(withText(R.string.managed_by_your_parent))
                .inRoot(withDecorView(not(mRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testShowManagedByParentToastMultipleCustodians() {
        ManagedPreferenceDelegate multiCustodianDelegate = new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return false;
            }

            @Override
            public boolean isPreferenceControlledByCustodian(Preference preference) {
                return true;
            }

            @Override
            public boolean doesProfileHaveMultipleCustodians() {
                return true;
            }
        };

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferencesUtils.showManagedByParentToast(
                    mRule.getActivity(), multiCustodianDelegate);
        });

        onView(withText(R.string.managed_by_your_parents))
                .inRoot(withDecorView(not(mRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testShowManagedSettingsCannotBeResetToast() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferencesUtils.showManagedSettingsCannotBeResetToast(mRule.getActivity());
        });

        onView(withText(R.string.managed_settings_cannot_be_reset))
                .inRoot(withDecorView(not(mRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }
}
