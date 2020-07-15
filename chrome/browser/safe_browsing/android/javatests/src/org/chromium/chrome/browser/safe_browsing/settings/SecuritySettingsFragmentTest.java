// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link SecuritySettingsFragment}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
// clang-format off
@Features.EnableFeatures({ChromeFeatureList.SAFE_BROWSING_SECURITY_SECTION_UI})
public class SecuritySettingsFragmentTest {
    // clang-format on
    private static final String ASSERT_SAFE_BROWSING_STATE_RADIO_BUTTON_GROUP =
            "Incorrect Safe Browsing state in the radio button group.";
    private static final String ASSERT_RADIO_BUTTON_CHECKED =
            "Incorrect radio button checked state.";
    private static final String ASSERT_SAFE_BROWSING_STATE_NATIVE =
            "Incorrect Safe Browsing state from native.";

    @Rule
    public SettingsActivityTestRule<SecuritySettingsFragment> mTestRule =
            new SettingsActivityTestRule<>(SecuritySettingsFragment.class);

    @Rule
    public TestRule mFeatureProcessor = new Features.InstrumentationProcessor();

    private RadioButtonGroupSafeBrowsingPreference mSafeBrowsingPreference;

    @Before
    public void setUp() {
        launchSettingsActivity();
    }

    private void launchSettingsActivity() {
        mTestRule.startSettingsActivity();
        SecuritySettingsFragment fragment = mTestRule.getFragment();
        mSafeBrowsingPreference =
                fragment.findPreference(SecuritySettingsFragment.PREF_SAFE_BROWSING);
        Assert.assertNotNull(
                "Safe Browsing preference should not be null.", mSafeBrowsingPreference);
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    public void testOnStartup() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            @SafeBrowsingState
            int currentState = SafeBrowsingBridge.getSafeBrowsingState();
            boolean enhanced_protection_checked =
                    currentState == SafeBrowsingState.ENHANCED_PROTECTION;
            boolean standard_protection_checked =
                    currentState == SafeBrowsingState.STANDARD_PROTECTION;
            boolean no_protection_checked = currentState == SafeBrowsingState.NO_SAFE_BROWSING;
            Assert.assertEquals(ASSERT_RADIO_BUTTON_CHECKED, enhanced_protection_checked,
                    getEnhancedProtectionButton().isChecked());
            Assert.assertEquals(ASSERT_RADIO_BUTTON_CHECKED, standard_protection_checked,
                    getStandardProtectionButton().isChecked());
            Assert.assertEquals(ASSERT_RADIO_BUTTON_CHECKED, no_protection_checked,
                    getNoProtectionButton().isChecked());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.EnableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    public void testCheckRadioButtons() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Click the Enhanced Protection button.
            getEnhancedProtectionButton().onClick(null);
            Assert.assertEquals(ASSERT_SAFE_BROWSING_STATE_RADIO_BUTTON_GROUP,
                    SafeBrowsingState.ENHANCED_PROTECTION, getSafeBrowsingState());
            Assert.assertTrue(
                    ASSERT_RADIO_BUTTON_CHECKED, getEnhancedProtectionButton().isChecked());
            Assert.assertFalse(
                    ASSERT_RADIO_BUTTON_CHECKED, getStandardProtectionButton().isChecked());
            Assert.assertFalse(ASSERT_RADIO_BUTTON_CHECKED, getNoProtectionButton().isChecked());
            Assert.assertEquals(ASSERT_SAFE_BROWSING_STATE_NATIVE,
                    SafeBrowsingState.ENHANCED_PROTECTION,
                    SafeBrowsingBridge.getSafeBrowsingState());

            // Click the Standard Protection button.
            getStandardProtectionButton().onClick(null);
            Assert.assertEquals(ASSERT_SAFE_BROWSING_STATE_RADIO_BUTTON_GROUP,
                    SafeBrowsingState.STANDARD_PROTECTION, getSafeBrowsingState());
            Assert.assertFalse(
                    ASSERT_RADIO_BUTTON_CHECKED, getEnhancedProtectionButton().isChecked());
            Assert.assertTrue(
                    ASSERT_RADIO_BUTTON_CHECKED, getStandardProtectionButton().isChecked());
            Assert.assertFalse(ASSERT_RADIO_BUTTON_CHECKED, getNoProtectionButton().isChecked());
            Assert.assertEquals(ASSERT_SAFE_BROWSING_STATE_NATIVE,
                    SafeBrowsingState.STANDARD_PROTECTION,
                    SafeBrowsingBridge.getSafeBrowsingState());

            // Click the No Protection button.
            getNoProtectionButton().onClick(null);
            Assert.assertEquals(ASSERT_SAFE_BROWSING_STATE_RADIO_BUTTON_GROUP,
                    SafeBrowsingState.NO_SAFE_BROWSING, getSafeBrowsingState());
            Assert.assertFalse(
                    ASSERT_RADIO_BUTTON_CHECKED, getEnhancedProtectionButton().isChecked());
            Assert.assertFalse(
                    ASSERT_RADIO_BUTTON_CHECKED, getStandardProtectionButton().isChecked());
            Assert.assertTrue(ASSERT_RADIO_BUTTON_CHECKED, getNoProtectionButton().isChecked());
            Assert.assertEquals(ASSERT_SAFE_BROWSING_STATE_NATIVE,
                    SafeBrowsingState.NO_SAFE_BROWSING, SafeBrowsingBridge.getSafeBrowsingState());
        });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @Features.DisableFeatures(ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED)
    public void testEnhancedProtectionDisabled() {
        Assert.assertNull(getEnhancedProtectionButton());
    }

    private @SafeBrowsingState int getSafeBrowsingState() {
        return mSafeBrowsingPreference.getSafeBrowsingStateForTesting();
    }

    private RadioButtonWithDescriptionAndAuxButton getEnhancedProtectionButton() {
        return mSafeBrowsingPreference.getEnhancedProtectionButtonForTesting();
    }

    private RadioButtonWithDescriptionAndAuxButton getStandardProtectionButton() {
        return mSafeBrowsingPreference.getStandardProtectionButtonForTesting();
    }

    private RadioButtonWithDescription getNoProtectionButton() {
        return mSafeBrowsingPreference.getNoProtectionButtonForTesting();
    }
}
