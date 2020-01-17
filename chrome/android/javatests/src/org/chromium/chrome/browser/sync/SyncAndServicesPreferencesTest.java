// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.SmallTest;
import android.support.test.uiautomator.UiDevice;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceCategory;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.ChromeSwitchPreference;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.sync.SyncAndServicesPreferences;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for SyncAndServicesPreferences.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncAndServicesPreferencesTest {
    private static final String TAG = "SyncAndServicesPreferencesTest";

    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

    private SettingsActivity mSettingsActivity;

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> ProfileSyncService.resetForTests());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncSwitch() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        final ChromeSwitchPreference syncSwitch = getSyncSwitch(fragment);

        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettings.get().isChromeSyncEnabled());
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertFalse(syncSwitch.isChecked());
        Assert.assertFalse(AndroidSyncSettings.get().isChromeSyncEnabled());
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettings.get().isChromeSyncEnabled());
    }

    /**
     * This is a regression test for http://crbug.com/454939.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testOpeningSettingsDoesntEnableSync() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.stopSync();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        closeFragment(fragment);
        Assert.assertFalse(AndroidSyncSettings.get().isChromeSyncEnabled());
    }

    /**
     * This is a regression test for http://crbug.com/467600.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testOpeningSettingsDoesntStartEngine() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.stopSync();
        startSyncAndServicesPreferences();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mSyncTestRule.getProfileSyncService().isSyncRequested());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testDefaultControlStatesWithSyncOffThenOn() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.stopSync();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        assertSyncOffState(fragment);
        mSyncTestRule.togglePreference(getSyncSwitch(fragment));
        SyncTestUtil.waitForEngineInitialized();
        assertSyncOnState(fragment);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testDefaultControlStatesWithSyncOnThenOff() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        assertSyncOnState(fragment);
        mSyncTestRule.togglePreference(getSyncSwitch(fragment));
        assertSyncOffState(fragment);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/991135")
    public void testSyncSwitchClearsServerAutofillCreditCards() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.setPaymentsIntegrationEnabled(true);

        Assert.assertFalse(
                "There should be no server cards", mSyncTestRule.hasServerAutofillCreditCards());
        mSyncTestRule.addServerAutofillCreditCard();
        Assert.assertTrue(
                "There should be server cards", mSyncTestRule.hasServerAutofillCreditCards());

        Assert.assertTrue(AndroidSyncSettings.get().isChromeSyncEnabled());
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        assertSyncOnState(fragment);
        ChromeSwitchPreference syncSwitch = getSyncSwitch(fragment);
        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettings.get().isChromeSyncEnabled());
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertFalse(syncSwitch.isChecked());
        Assert.assertFalse(AndroidSyncSettings.get().isChromeSyncEnabled());

        closeFragment(fragment);

        Assert.assertFalse("There should be no server cards remaining",
                mSyncTestRule.hasServerAutofillCreditCards());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testDismissedSettingsDoesNotSetFirstSetupComplete() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        // FirstSetupComplete should not be set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(ProfileSyncService.get().isFirstSetupComplete()); });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testDismissedSettingsShowsSyncSwitchOffByDefault() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        assertSyncOffState(fragment);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testDismissedSettingsShowsSyncErrorCard() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        Assert.assertNotNull("Sync error card should be shown", getSyncErrorCard(fragment));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testFirstSetupCompleteIsSetAfterSettingsOpenedAndBackPressed() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        // Open Settings and leave sync off.
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        pressBackAndDismissActivity(fragment.getActivity());
        // FirstSetupComplete should be set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(ProfileSyncService.get().isFirstSetupComplete()); });
        fragment = startSyncAndServicesPreferences();
        Assert.assertNull("Sync error card should not be shown", getSyncErrorCard(fragment));
        assertSyncOffState(fragment);
        Assert.assertFalse(AndroidSyncSettings.get().isChromeSyncEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testFirstSetupCompleteIsSetAfterSettingsOpenedAndDismissed() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        // Open Settings and leave sync off.
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        ApplicationTestUtils.finishActivity(fragment.getActivity());
        // FirstSetupComplete should be set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(ProfileSyncService.get().isFirstSetupComplete()); });
        fragment = startSyncAndServicesPreferences();
        Assert.assertNull("Sync error card should not be shown", getSyncErrorCard(fragment));
        assertSyncOffState(fragment);
        Assert.assertFalse(AndroidSyncSettings.get().isChromeSyncEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testFirstSetupCompleteIsSetAfterSyncTurnedOn() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        // Open Settings and turn sync on.
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        ChromeSwitchPreference syncSwitch = getSyncSwitch(fragment);
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettings.get().isChromeSyncEnabled());
        // FirstSetupComplete should be set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(ProfileSyncService.get().isFirstSetupComplete()); });
        Assert.assertNull("Sync error card should not be shown", getSyncErrorCard(fragment));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testTrustedVaultKeyRequiredShowsSyncErrorCard() throws Exception {
        final FakeProfileSyncService pss = overrideProfileSyncService();
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        pss.setEngineInitialized(true);
        pss.setTrustedVaultKeyRequiredForPreferredDataTypes(true);

        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();

        Assert.assertNotNull("Sync error card should be shown", getSyncErrorCard(fragment));
    }

    /**
     * Test: if the onboarding was never shown, the AA chrome preference should not exist.
     *
     * Note: presence of the {@link SyncAndServicesPreferences.PREF_AUTOFILL_ASSISTANT}
     * shared preference indicates whether onboarding was shown or not.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    public void testAutofillAssistantNoPreferenceIfOnboardingNeverShown() {
        final SyncAndServicesPreferences syncPrefs = startSyncAndServicesPreferences();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(
                    syncPrefs.findPreference(SyncAndServicesPreferences.PREF_AUTOFILL_ASSISTANT));
        });
    }

    /**
     * Test: if the onboarding was shown at least once, the AA chrome preference should also exist.
     *
     * Note: presence of the {@link SyncAndServicesPreferences.PREF_AUTOFILL_ASSISTANT}
     * shared preference indicates whether onboarding was shown or not.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    public void testAutofillAssistantPreferenceShownIfOnboardingShown() {
        setAutofillAssistantSwitchValue(true);
        final SyncAndServicesPreferences syncPrefs = startSyncAndServicesPreferences();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNotNull(
                    syncPrefs.findPreference(SyncAndServicesPreferences.PREF_AUTOFILL_ASSISTANT));
        });
    }

    /**
     * Ensure that the "Autofill Assistant" setting is not shown when the feature is disabled.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    public void testAutofillAssistantNoPreferenceIfFeatureDisabled() {
        setAutofillAssistantSwitchValue(true);
        final SyncAndServicesPreferences syncPrefs = startSyncAndServicesPreferences();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(
                    syncPrefs.findPreference(SyncAndServicesPreferences.PREF_AUTOFILL_ASSISTANT));
        });
    }

    /**
     * Ensure that the "Autofill Assistant" on/off switch works.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    public void testAutofillAssistantSwitchOn() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setAutofillAssistantSwitchValue(true); });
        final SyncAndServicesPreferences syncAndServicesPreferences =
                startSyncAndServicesPreferences();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference autofillAssistantSwitch =
                    (ChromeSwitchPreference) syncAndServicesPreferences.findPreference(
                            SyncAndServicesPreferences.PREF_AUTOFILL_ASSISTANT);
            Assert.assertTrue(autofillAssistantSwitch.isChecked());

            autofillAssistantSwitch.performClick();
            Assert.assertFalse(syncAndServicesPreferences.isAutofillAssistantSwitchOn());
            autofillAssistantSwitch.performClick();
            Assert.assertTrue(syncAndServicesPreferences.isAutofillAssistantSwitchOn());
        });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    public void testAutofillAssistantSwitchOff() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setAutofillAssistantSwitchValue(false); });
        final SyncAndServicesPreferences syncAndServicesPreferences =
                startSyncAndServicesPreferences();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference autofillAssistantSwitch =
                    (ChromeSwitchPreference) syncAndServicesPreferences.findPreference(
                            SyncAndServicesPreferences.PREF_AUTOFILL_ASSISTANT);
            Assert.assertFalse(autofillAssistantSwitch.isChecked());
        });
    }

    private void setAutofillAssistantSwitchValue(boolean newValue) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, newValue);
    }

    // TODO(crbug.com/1030725): SyncTestRule should support overriding ProfileSyncService.
    private FakeProfileSyncService overrideProfileSyncService() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            // PSS has to be constructed on the UI thread.
            FakeProfileSyncService fakeProfileSyncService = new FakeProfileSyncService();
            ProfileSyncService.overrideForTests(fakeProfileSyncService);
            return fakeProfileSyncService;
        });
    }

    /**
     * Start SyncAndServicesPreferences signin screen and dissmiss it without pressing confirm or
     * cancel.
     */
    private void startPreferencesForAdvancedSyncFlowAndInterruptIt() throws Exception {
        Context context = InstrumentationRegistry.getTargetContext();
        String fragmentName = SyncAndServicesPreferences.class.getName();
        final Bundle arguments = SyncAndServicesPreferences.createArguments(true);
        Intent intent =
                SettingsLauncher.createIntentForSettingsPage(context, fragmentName, arguments);
        Activity activity = InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
        Assert.assertTrue(activity instanceof SettingsActivity);
        ApplicationTestUtils.finishActivity(activity);
    }

    private SyncAndServicesPreferences startSyncAndServicesPreferences() {
        mSettingsActivity =
                mSyncTestRule.startSettingsActivity(SyncAndServicesPreferences.class.getName());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (SyncAndServicesPreferences) mSettingsActivity.getMainFragment();
    }

    private void closeFragment(SyncAndServicesPreferences fragment) {
        FragmentTransaction transaction =
                mSettingsActivity.getSupportFragmentManager().beginTransaction();
        transaction.remove(fragment);
        transaction.commit();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void pressBackAndDismissActivity(Activity activity) throws Exception {
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.pressBack();
        ApplicationTestUtils.finishActivity(activity);
    }

    private ChromeSwitchPreference getSyncSwitch(SyncAndServicesPreferences fragment) {
        return (ChromeSwitchPreference) fragment.findPreference(
                SyncAndServicesPreferences.PREF_SYNC_REQUESTED);
    }

    private Preference getSyncErrorCard(SyncAndServicesPreferences fragment) {
        return ((PreferenceCategory) fragment.findPreference(
                        SyncAndServicesPreferences.PREF_SYNC_CATEGORY))
                .findPreference(SyncAndServicesPreferences.PREF_SYNC_ERROR_CARD);
    }

    private void assertSyncOnState(SyncAndServicesPreferences fragment) {
        Assert.assertTrue("The sync switch should be on.", getSyncSwitch(fragment).isChecked());
        Assert.assertTrue(
                "The sync switch should be enabled.", getSyncSwitch(fragment).isEnabled());
    }

    private void assertSyncOffState(SyncAndServicesPreferences fragment) {
        Assert.assertFalse("The sync switch should be off.", getSyncSwitch(fragment).isChecked());
        Assert.assertTrue(
                "The sync switch should be enabled.", getSyncSwitch(fragment).isEnabled());
    }
}
