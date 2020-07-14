// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;

import androidx.preference.Preference;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests {@link SafetyCheckSettingsFragment} together with {@link SafetyCheckViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SafetyCheckSettingsFragmentTest {
    private static final String PASSWORDS = "passwords";
    private static final String SAFE_BROWSING = "safe_browsing";
    private static final String UPDATES = "updates";
    @Rule
    public SettingsActivityTestRule<SafetyCheckSettingsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(SafetyCheckSettingsFragment.class);

    @Mock
    private SafetyCheckBridge mSafetyCheckBridge;

    private PropertyModel mModel;
    private SafetyCheckSettingsFragment mFragment;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    public void testGetSafetyCheckSettingsElementTitleNew() {
        SharedPreferencesManager preferenceManager = SharedPreferencesManager.getInstance();
        // The "NEW" label should still be shown after one run.
        preferenceManager.writeInt(ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_RUN_COUNTER, 1);
        String title = SafetyCheckSettingsFragment
                               .getSafetyCheckSettingsElementTitle(
                                       InstrumentationRegistry.getTargetContext())
                               .toString();
        assertTrue(title.contains("New"));
    }

    @Test
    @SmallTest
    public void testGetSafetyCheckSettingsElementTitleNoNew() {
        SharedPreferencesManager preferenceManager = SharedPreferencesManager.getInstance();
        // The "NEW" label disappears after 3 runs.
        preferenceManager.writeInt(ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_RUN_COUNTER, 3);
        String title = SafetyCheckSettingsFragment
                               .getSafetyCheckSettingsElementTitle(
                                       InstrumentationRegistry.getTargetContext())
                               .toString();
        assertFalse(title.contains("New"));
    }

    private void createFragmentAndModel() {
        mSettingsActivityTestRule.startSettingsActivity();
        mFragment = (SafetyCheckSettingsFragment) mSettingsActivityTestRule.getFragment();
        mModel = SafetyCheckCoordinator.createModelAndMcp(mFragment);
    }

    @Test
    @MediumTest
    public void testNullStateDisplayedCorrectly() {
        createFragmentAndModel();
        Preference passwords = mFragment.findPreference(PASSWORDS);
        Preference safeBrowsing = mFragment.findPreference(SAFE_BROWSING);
        Preference updates = mFragment.findPreference(UPDATES);

        assertEquals("", passwords.getSummary());
        assertEquals("", safeBrowsing.getSummary());
        assertEquals("", updates.getSummary());
    }

    @Test
    @MediumTest
    public void testStateChangeDisplayedCorrectly() {
        createFragmentAndModel();
        Preference passwords = mFragment.findPreference(PASSWORDS);
        Preference safeBrowsing = mFragment.findPreference(SAFE_BROWSING);
        Preference updates = mFragment.findPreference(UPDATES);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Passwords state remains unchanged.
            // Safe browsing is in "checking".
            mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.CHECKING);
            // Updates goes through "checking" and ends up in "outdated".
            mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.CHECKING);
            mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.OUTDATED);
        });

        assertEquals("", passwords.getSummary());
        assertEquals("", safeBrowsing.getSummary());
        assertEquals(InstrumentationRegistry.getTargetContext().getString(
                             R.string.safety_check_updates_outdated),
                updates.getSummary());
    }
}
