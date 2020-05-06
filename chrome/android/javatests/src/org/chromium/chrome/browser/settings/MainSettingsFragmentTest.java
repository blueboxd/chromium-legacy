// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.accounts.Account;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.accessibility.settings.AccessibilitySettings;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillProfilesFragment;
import org.chromium.chrome.browser.datareduction.settings.DataReductionPreferenceFragment;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.language.settings.LanguageSettings;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.notifications.settings.NotificationSettings;
import org.chromium.chrome.browser.password_manager.settings.PasswordSettings;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.sync.settings.SignInPreference;
import org.chromium.chrome.browser.sync.settings.SignInPreference.State;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Test for {@link MainSettings}. Main purpose is to have a sanity check on the xml.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
public class MainSettingsFragmentTest {
    private static final String SEARCH_ENGINE_SHORT_NAME = "Google";

    @Rule
    public HomepageTestRule mHomepageTestRule = new HomepageTestRule();
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();
    @Rule
    public SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    @Mock
    public TemplateUrlService mMockTemplateUrlService;
    @Mock
    public TemplateUrl mMockSearchEngine;

    private @Nullable TemplateUrlService mActualTemplateUrlService;

    private MainSettings mMainSettings;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        DeveloperSettings.setIsEnabledForTests(true);
        NightModeUtils.setNightModeSupportedForTesting(true);
    }

    @After
    public void tearDown() {
        DeveloperSettings.setIsEnabledForTests(null);
        NightModeUtils.setNightModeSupportedForTesting(null);
        if (mActualTemplateUrlService != null) {
            // Reset the actual service if the mock is used.
            TemplateUrlServiceFactory.setInstanceForTesting(mActualTemplateUrlService);
        }
    }

    private void launchSettingsActivity() {
        mSettingsActivityTestRule.startSettingsActivity();
        mMainSettings = mSettingsActivityTestRule.getFragment();
        Assert.assertNotNull("SettingsActivity failed to launch.", mMainSettings);
    }

    private void configureMockSearchEngine() {
        // Cache the actual Url Service, so the test can put it back after tests.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActualTemplateUrlService = TemplateUrlServiceFactory.get(); });

        TemplateUrlServiceFactory.setInstanceForTesting(mMockTemplateUrlService);
        Mockito.doReturn(mMockSearchEngine)
                .when(mMockTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();

        Mockito.doReturn(SEARCH_ENGINE_SHORT_NAME).when(mMockSearchEngine).getShortName();
    }

    /**
     * Test for the "Account" section.
     */
    @Test
    @SmallTest
    public void testStartup() {
        launchSettingsActivity();

        // For non-signed-in users, the section contains the generic header.
        assertSettingsExists(MainSettings.PREF_SIGN_IN, null);
        assertSettingsExists(MainSettings.PREF_SYNC_AND_SERVICES, SyncAndServicesSettings.class);

        // SignInPreference status check.
        // As the user is not signed in, sign in promo will show, section header will be hidden.
        Assert.assertNull("Account section header should be hidden.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_SECTION));

        // Assert for "Basics" section
        assertSettingsExists(MainSettings.PREF_SEARCH_ENGINE, SearchEngineSettings.class);
        assertSettingsExists(MainSettings.PREF_PASSWORDS, PasswordSettings.class);
        assertSettingsExists("autofill_payment_methods", AutofillPaymentMethodsFragment.class);
        assertSettingsExists("autofill_addresses", AutofillProfilesFragment.class);
        assertSettingsExists(MainSettings.PREF_NOTIFICATIONS, NotificationSettings.class);
        assertSettingsExists(MainSettings.PREF_HOMEPAGE, HomepageSettings.class);
        assertSettingsExists(MainSettings.PREF_UI_THEME, ThemeSettingsFragment.class);

        // Verification for summary for the search engine and the homepage
        Assert.assertEquals("Homepage summary is different than homepage state",
                mMainSettings.getString(R.string.text_on),
                mMainSettings.findPreference(MainSettings.PREF_HOMEPAGE).getSummary().toString());

        // Assert for advanced section
        assertSettingsExists("privacy", PrivacySettings.class);
        assertSettingsExists("accessibility", AccessibilitySettings.class);
        assertSettingsExists("content_settings", SiteSettings.class);
        assertSettingsExists("languages", LanguageSettings.class);
        assertSettingsExists(
                MainSettings.PREF_DATA_REDUCTION, DataReductionPreferenceFragment.class);
        assertSettingsExists(MainSettings.PREF_DOWNLOADS, DownloadSettings.class);
        assertSettingsExists(MainSettings.PREF_DEVELOPER, DeveloperSettings.class);
        assertSettingsExists("about_chrome", AboutChromeSettings.class);
    }

    @Test
    @SmallTest
    public void testHomepageOff() {
        mHomepageTestRule.disableHomepageForTest();
        launchSettingsActivity();

        // Verification for summary for the search engine and the homepage
        Assert.assertEquals("Homepage summary is different than homepage state",
                mMainSettings.getString(R.string.text_off),
                mMainSettings.findPreference(MainSettings.PREF_HOMEPAGE).getSummary().toString());
    }

    @Test
    @SmallTest
    public void testSearchEngineDisabled() {
        Mockito.doReturn(false).when(mMockTemplateUrlService).isLoaded();
        configureMockSearchEngine();

        launchSettingsActivity();
        Preference searchEngineSettings =
                assertSettingsExists(MainSettings.PREF_SEARCH_ENGINE, SearchEngineSettings.class);
        // Verification for summary for the search engine and the homepage
        Assert.assertFalse("Search Engine preference should be disabled when service is not ready.",
                searchEngineSettings.isEnabled());
        Assert.assertTrue("Search Engine preference should be disabled when service is not ready.",
                TextUtils.isEmpty(searchEngineSettings.getSummary()));
    }

    /**
     * Test when the sign-in preference is the promo. The section header should be hidden.
     */
    @Test
    @SmallTest
    public void testAccountSignIn() throws InterruptedException {
        Account account = mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.signOut();

        launchSettingsActivity();

        SignInPreference signInPreference =
                (SignInPreference) assertSettingsExists(MainSettings.PREF_SIGN_IN, null);
        Assert.assertEquals("SignInPreference should be at the personalized promo state. ",
                signInPreference.getState(), State.PERSONALIZED_PROMO);
        Assert.assertNull("Account section header should be hidden when promo is shown.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_SECTION));

        // SignIn to see the changes
        mSyncTestRule.signinAndEnableSync(account);
        SyncTestUtil.waitForSyncActive();
        Assert.assertEquals("SignInPreference should be at the personalized promo state. ",
                signInPreference.getState(), State.SIGNED_IN);
        Assert.assertNotNull("Account section header should appear when user signed in.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_SECTION));
    }

    @Test
    @SmallTest
    public void testRemoveSettings() {
        // Disable night mode
        NightModeUtils.setNightModeSupportedForTesting(false);

        // Disable developer option
        DeveloperSettings.setIsEnabledForTests(false);

        launchSettingsActivity();

        Assert.assertNull("Preference should be disabled: " + MainSettings.PREF_UI_THEME,
                mMainSettings.findPreference(MainSettings.PREF_UI_THEME));
        Assert.assertNull("Preference should be disabled: " + MainSettings.PREF_DEVELOPER,
                mMainSettings.findPreference(MainSettings.PREF_DEVELOPER));
    }

    /**
     * Assert the target preference exists in the main settings and creates the expected fragment,
     * then return that preference.
     *
     * @param prefKey preference key for {@link
     *         androidx.preference.PreferenceFragmentCompat#findPreference(CharSequence)}
     * @param settingsFragmentClass class name that the target preference is holding
     * @return the target preference if exists, raise {@link AssertionError} otherwise.
     */
    private Preference assertSettingsExists(String prefKey, @Nullable Class settingsFragmentClass) {
        Preference pref = mMainSettings.findPreference(prefKey);
        Assert.assertNotNull("Preference is missing: " + prefKey, pref);

        if (settingsFragmentClass == null) return pref;

        try {
            Assert.assertNotNull(
                    "Fragment attached to the preference is null.", pref.getFragment());
            Assert.assertEquals("Preference class is different.", settingsFragmentClass,
                    Class.forName(pref.getFragment()));
        } catch (ClassNotFoundException e) {
            throw new AssertionError("Pref fragment <" + pref.getFragment() + "> is not found.");
        }
        return pref;
    }
}
