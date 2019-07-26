// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.v4.app.FragmentTransaction;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreferenceCompat;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.sync.SyncAndServicesPreferences;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
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

    private Preferences mPreferences;

    @After
    public void tearDown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> ProfileSyncService.resetForTests());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncSwitch() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        final ChromeSwitchPreferenceCompat syncSwitch = getSyncSwitch(fragment);

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
        ChromeSwitchPreferenceCompat syncSwitch = getSyncSwitch(fragment);
        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettings.get().isChromeSyncEnabled());
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertFalse(syncSwitch.isChecked());
        Assert.assertFalse(AndroidSyncSettings.get().isChromeSyncEnabled());

        closeFragment(fragment);

        Assert.assertFalse("There should be no server cards remaining",
                mSyncTestRule.hasServerAutofillCreditCards());
    }

    private SyncAndServicesPreferences startSyncAndServicesPreferences() {
        mPreferences = mSyncTestRule.startPreferences(SyncAndServicesPreferences.class.getName());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (SyncAndServicesPreferences) mPreferences.getMainFragmentCompat();
    }

    private void closeFragment(SyncAndServicesPreferences fragment) {
        FragmentTransaction transaction =
                mPreferences.getSupportFragmentManager().beginTransaction();
        transaction.remove(fragment);
        transaction.commit();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private ChromeSwitchPreferenceCompat getSyncSwitch(SyncAndServicesPreferences fragment) {
        return (ChromeSwitchPreferenceCompat) fragment.findPreference(
                SyncAndServicesPreferences.PREF_SYNC_REQUESTED);
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