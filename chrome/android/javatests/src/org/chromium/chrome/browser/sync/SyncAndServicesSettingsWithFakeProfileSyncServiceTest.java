// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;

/**
 * Tests for SyncAndServicesSettings with FakeProfileSyncService.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncAndServicesSettingsWithFakeProfileSyncServiceTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule() {
        @Override
        protected FakeProfileSyncService createProfileSyncService() {
            return new FakeProfileSyncService();
        }
    };

    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    public void testTrustedVaultKeyRequiredShowsSyncErrorCard() throws Exception {
        FakeProfileSyncService fakeProfileSyncService =
                (FakeProfileSyncService) mSyncTestRule.getProfileSyncService();
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        fakeProfileSyncService.setEngineInitialized(true);
        fakeProfileSyncService.setTrustedVaultKeyRequiredForPreferredDataTypes(true);

        SyncAndServicesSettings fragment = startSyncAndServicesPreferences();

        Assert.assertNotNull("Sync error card should be shown", getSyncErrorCard(fragment));
    }

    private SyncAndServicesSettings startSyncAndServicesPreferences() {
        SettingsActivity settingsActivity =
                mSyncTestRule.startSettingsActivity(SyncAndServicesSettings.class.getName());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (SyncAndServicesSettings) settingsActivity.getMainFragment();
    }

    private Preference getSyncErrorCard(SyncAndServicesSettings fragment) {
        return ((PreferenceCategory) fragment.findPreference(
                        SyncAndServicesSettings.PREF_SYNC_CATEGORY))
                .findPreference(SyncAndServicesSettings.PREF_SYNC_ERROR_CARD);
    }
}
