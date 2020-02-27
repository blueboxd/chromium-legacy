// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.homepage;

import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.settings.ChromeSwitchPreference;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.TextMessagePreference;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarConfiguration;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithEditText;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Test for {@link HomepageSettings}.
 *
 * This test is created to check whether the UI components and the interaction when
 * {@link ChromeFeatureList#HOMEPAGE_SETTINGS_UI_CONVERSION } is enabled. Tests for {@link
 * HomepageSettings} when feature flag is turned off can be found in {@link
 * HomepageSettingsFragmentWithEditorTest}, {@link
 * org.chromium.chrome.browser.partnercustomizations.PartnerHomepageIntegrationTest} and {@link
 * org.chromium.chrome.browser.homepage.HomepagePolicyIntegrationTest}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
// clang-format off
@Features.EnableFeatures(ChromeFeatureList.HOMEPAGE_SETTINGS_UI_CONVERSION)
@Features.DisableFeatures(ChromeFeatureList.CHROME_DUET)
public class HomepageSettingsFragmentTest {
    // clang-format on
    private static final String ASSERT_MESSAGE_SWITCH_ENABLE = "Switch should be enabled.";
    private static final String ASSERT_MESSAGE_SWITCH_DISABLE = "Switch should be disabled.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_ENABLED =
            "RadioButton should be enabled.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_DISABLED =
            "RadioButton should be disabled.";
    private static final String ASSERT_MESSAGE_TITLE_ENABLED =
            "Title for RadioButtonGroup should be enabled.";
    private static final String ASSERT_MESSAGE_TITLE_DISABLED =
            "Title for RadioButtonGroup should be disabled.";

    private static final String ASSERT_MESSAGE_SWITCH_CHECK =
            "Switch preference state is not consistent with test settings.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK =
            "NTP Radio button does not check the expected option in test settings.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK =
            "Customized Radio button does not check the expected option in test settings.";
    private static final String ASSERT_MESSAGE_EDIT_TEXT =
            "EditText does not contains the expected homepage in test settings.";
    private static final String ASSERT_HOMEPAGE_MANAGER_SETTINGS =
            "HomepageManager#getHomepageUri is different than test homepage settings.";

    private static final String ASSERT_MESSAGE_DUET_SWITCH_INVISIBLE =
            "Switch should not be visible when duet is enabled.";

    private static final String TEST_URL_FOO = "http://127.0.0.1:8000/foo.html";
    private static final String TEST_URL_BAR = "http://127.0.0.1:8000/bar.html";
    private static final String CHROME_NTP = UrlConstants.NTP_NON_NATIVE_URL;

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    @Rule
    public TestRule mFeatureProcessor = new Features.InstrumentationProcessor();

    @Mock
    public HomepagePolicyManager mMockPolicyManager;

    private ChromeSwitchPreference mSwitch;
    private RadioButtonGroupHomepagePreference mRadioGroupPreference;
    private TextMessagePreference mManagedText;

    private TextView mTitleTextView;
    private RadioButtonWithDescription mChromeNtpRadioButton;
    private RadioButtonWithEditText mCustomUriRadioButton;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        RecordHistogram.setDisabledForTests(true);
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
    }

    private void launchSettingsActivity() {
        SettingsActivity homepagePreferenceActivity =
                mTestRule.startSettingsActivity(HomepageSettings.class.getName());
        HomepageSettings fragment = (HomepageSettings) homepagePreferenceActivity.getMainFragment();
        Assert.assertNotNull(fragment);

        mSwitch = (ChromeSwitchPreference) fragment.findPreference(
                HomepageSettings.PREF_HOMEPAGE_SWITCH);
        mRadioGroupPreference = (RadioButtonGroupHomepagePreference) fragment.findPreference(
                HomepageSettings.PREF_HOMEPAGE_RADIO_GROUP);
        mManagedText =
                (TextMessagePreference) fragment.findPreference(HomepageSettings.PREF_TEXT_MANAGED);

        Assert.assertTrue(
                "RadioGroupPreference should be visible when Homepage Conversion is enabled.",
                mRadioGroupPreference.isVisible());

        mTitleTextView = mRadioGroupPreference.getTitleTextView();
        mChromeNtpRadioButton = mRadioGroupPreference.getChromeNTPRadioButton();
        mCustomUriRadioButton = mRadioGroupPreference.getCustomUriRadioButton();

        Assert.assertNotNull("Title text view is null.", mTitleTextView);
        Assert.assertNotNull("Chrome NTP radio button is null.", mChromeNtpRadioButton);
        Assert.assertNotNull("Custom URI radio button is null.", mCustomUriRadioButton);
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_ChromeNTP() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);
        mHomepageTestRule.useChromeNTPForTest();

        launchSettingsActivity();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest(message = "crbug.com/1051213")
    public void testStartUp_ChromeNTP_BottomToolbar() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);
        mHomepageTestRule.useChromeNTPForTest();

        CachedFeatureFlags.setForTesting(ChromeFeatureList.CHROME_DUET, true);
        Assert.assertTrue("BottomToolbar should be enabled after setting up feature flag.",
                BottomToolbarConfiguration.isBottomToolbarEnabled());

        launchSettingsActivity();

        Assert.assertFalse(ASSERT_MESSAGE_DUET_SWITCH_INVISIBLE, mSwitch.isVisible());

        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());

        CachedFeatureFlags.setForTesting(ChromeFeatureList.CHROME_DUET, false);
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_Customized() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);

        launchSettingsActivity();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    @DisabledTest
    public void testStartUp_Policies_Customized() {
        // Set mock policies
        Mockito.when(mMockPolicyManager.isHomepageLocationPolicyEnabled()).thenReturn(true);
        Mockito.when(mMockPolicyManager.getHomepagePreference()).thenReturn(TEST_URL_BAR);

        HomepagePolicyManager origInstance = HomepagePolicyManager.getInstance();
        HomepagePolicyManager.setInstanceForTests(mMockPolicyManager);
        CachedFeatureFlags.setForTesting(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY, true);

        launchSettingsActivity();

        // When policy enabled, all components should be disabled.
        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_TITLE_DISABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());

        // Additional verification - text message should be displayed, NTP button should be hidden.
        Assert.assertEquals("NTP Button should not be visible.", View.GONE,
                mChromeNtpRadioButton.getVisibility());
        Assert.assertEquals("Customized Button should be visible.", View.VISIBLE,
                mCustomUriRadioButton.getVisibility());
        Assert.assertFalse(
                "Managed text message preference should be in visible when duet is disabled.",
                mManagedText.isVisible());

        // Reset policy
        HomepagePolicyManager.setInstanceForTests(origInstance);
        CachedFeatureFlags.setForTesting(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY, null);
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    @DisabledTest
    public void testStartUp_Policies_NTP() {
        // Set mock policies
        Mockito.when(mMockPolicyManager.isHomepageLocationPolicyEnabled()).thenReturn(true);
        Mockito.when(mMockPolicyManager.getHomepagePreference()).thenReturn(CHROME_NTP);

        HomepagePolicyManager origInstance = HomepagePolicyManager.getInstance();
        HomepagePolicyManager.setInstanceForTests(mMockPolicyManager);
        CachedFeatureFlags.setForTesting(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY, true);

        launchSettingsActivity();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_TITLE_DISABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());

        // Additional verification - customized radio button should be disabled.
        Assert.assertEquals("NTP Button should be visible.", View.VISIBLE,
                mChromeNtpRadioButton.getVisibility());
        Assert.assertEquals("Customized Button should not be visible.", View.GONE,
                mCustomUriRadioButton.getVisibility());
        Assert.assertFalse(
                "Managed text message preference should be invisible when duet is disabled.",
                mManagedText.isVisible());

        // Reset policy and feature flags
        HomepagePolicyManager.setInstanceForTests(origInstance);
        CachedFeatureFlags.setForTesting(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY, null);
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest
    public void testStartUp_Policies_Customized_BottomToolbar() {
        // Set mock policies
        Mockito.when(mMockPolicyManager.isHomepageLocationPolicyEnabled()).thenReturn(true);
        Mockito.when(mMockPolicyManager.getHomepagePreference()).thenReturn(TEST_URL_BAR);

        HomepagePolicyManager origInstance = HomepagePolicyManager.getInstance();
        HomepagePolicyManager.setInstanceForTests(mMockPolicyManager);
        CachedFeatureFlags.setForTesting(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY, true);

        CachedFeatureFlags.setForTesting(ChromeFeatureList.CHROME_DUET, true);
        Assert.assertTrue("BottomToolbar should be enabled after setting up feature flag.",
                BottomToolbarConfiguration.isBottomToolbarEnabled());

        launchSettingsActivity();

        Assert.assertFalse(ASSERT_MESSAGE_DUET_SWITCH_INVISIBLE, mSwitch.isVisible());

        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_TITLE_DISABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mCustomUriRadioButton.isChecked());

        // Additional verification - managed text should be visible
        Assert.assertEquals("NTP Button should not be visible.", View.GONE,
                mChromeNtpRadioButton.getVisibility());
        Assert.assertEquals("Customized Button should not be visible.", View.VISIBLE,
                mCustomUriRadioButton.getVisibility());
        Assert.assertTrue("Managed text message preference should be visible when duet enabled.",
                mManagedText.isVisible());
        Assert.assertFalse(
                "Managed text message preference should be disabled.", mManagedText.isEnabled());

        // Reset policy
        HomepagePolicyManager.setInstanceForTests(origInstance);
        CachedFeatureFlags.setForTesting(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY, null);
        CachedFeatureFlags.setForTesting(ChromeFeatureList.CHROME_DUET, null);
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_DefaultToPartner() {
        String origPartnerHomepage = PartnerBrowserCustomizations.getHomePageUrl();
        PartnerBrowserCustomizations.setHomepageForTests(TEST_URL_FOO);
        mHomepageTestRule.useDefaultHomepageForTest();

        launchSettingsActivity();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        // Reset partner provided information
        PartnerBrowserCustomizations.setHomepageForTests(origPartnerHomepage);
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_DefaultToNTP() {
        mHomepageTestRule.useDefaultHomepageForTest();

        launchSettingsActivity();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());

        // When no default homepage provided, the string should just be empty.
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT, "", mCustomUriRadioButton.getPrimaryText().toString());
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_HomepageDisabled() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);
        mHomepageTestRule.disableHomepageForTest();

        launchSettingsActivity();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_TITLE_DISABLED, mTitleTextView.isEnabled());

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());

        Assert.assertNull(ASSERT_HOMEPAGE_MANAGER_SETTINGS, HomepageManager.getHomepageUri());
    }

    /**
     * Test toggle switch to enable/disable homepage.
     */
    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testToggleSwitch() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_FOO);
        mHomepageTestRule.useChromeNTPForTest();

        launchSettingsActivity();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue("Homepage should be enabled.", HomepageManager.isHomepageEnabled());

        // Check the widget status
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        // Click the switch
        TestThreadUtils.runOnUiThreadBlocking(() -> mSwitch.performClick());
        Assert.assertFalse("After toggle the switch, " + ASSERT_MESSAGE_TITLE_DISABLED,
                mTitleTextView.isEnabled());
        Assert.assertFalse("After toggle the switch, " + ASSERT_MESSAGE_RADIO_BUTTON_DISABLED,
                mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse("After toggle the switch, " + ASSERT_MESSAGE_RADIO_BUTTON_DISABLED,
                mCustomUriRadioButton.isEnabled());
        Assert.assertFalse("Homepage should be disabled after toggle switch.",
                HomepageManager.isHomepageEnabled());

        // Check the widget status - everything should remain unchanged.
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        TestThreadUtils.runOnUiThreadBlocking(() -> mSwitch.performClick());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue("Homepage should be enabled again.", HomepageManager.isHomepageEnabled());

        // Check the widget status - everything should remain unchanged.
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());
    }

    /**
     * Test checking different radio button to change the homepage.
     */
    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testCheckRadioButtons() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_FOO);
        launchSettingsActivity();

        // Initial state check
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS, TEST_URL_FOO, HomepageManager.getHomepageUri());

        // Check radio button to select NTP as homepage
        checkRadioButtonAndWait(mChromeNtpRadioButton);

        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertTrue(ASSERT_HOMEPAGE_MANAGER_SETTINGS,
                NewTabPage.isNTPUrl(HomepageManager.getHomepageUri()));

        // Check back to customized radio button
        checkRadioButtonAndWait(mCustomUriRadioButton);

        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS, TEST_URL_FOO, HomepageManager.getHomepageUri());
    }

    /**
     * Test if changing uris in EditText will change homepage accordingly.
     */
    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testChangeCustomized() {
        mHomepageTestRule.useChromeNTPForTest();
        launchSettingsActivity();

        // Initial state check
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT, "", mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertTrue(ASSERT_HOMEPAGE_MANAGER_SETTINGS,
                NewTabPage.isNTPUrl(HomepageManager.getHomepageUri()));

        // Update the text box. To do this, request focus for customized radio button so that the
        // checked option will be changed.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCustomUriRadioButton.requestFocus();
            mCustomUriRadioButton.setPrimaryText(TEST_URL_FOO);
        });

        // Radio Button should switched to customized homepage.
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS, TEST_URL_FOO, HomepageManager.getHomepageUri());

        // Update the text box, homepage should change accordingly.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mCustomUriRadioButton.setPrimaryText(TEST_URL_BAR));
        Assert.assertEquals(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS, TEST_URL_BAR, HomepageManager.getHomepageUri());
    }

    private void checkRadioButtonAndWait(RadioButtonWithDescription radioButton) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { TouchCommon.singleClickView(radioButton, 5, 5); });
        CriteriaHelper.pollUiThread(radioButton::isChecked, "Radio button is never checked.");
    }
}
