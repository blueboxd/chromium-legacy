// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.SmallTest;

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

import org.chromium.base.Promise;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;

/**
 * Test for ManageSyncSettings with FakeProfileSyncService.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManageSyncSettingsWithFakeProfileSyncServiceTest {
    private static final long TRUSTED_VAULT_CLIENT_NATIVE_PTR = 12345;

    /**
     * Simple activity that mimics a trusted vault key retrieval flow that succeeds immediately.
     */
    public static class DummyKeyRetrievalActivity extends Activity {
        @Override
        protected void onCreate(@Nullable Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setResult(RESULT_OK);
            finish();
        }
    };

    /**
     * Stub backend for TrustedVaultClient used to test the key retrieval flow.
     */
    private static class FakeTrustedVaultClientBackend implements TrustedVaultClient.Backend {
        public FakeTrustedVaultClientBackend() {}

        @Override
        public Promise<List<byte[]>> fetchKeys(CoreAccountInfo accountInfo) {
            return Promise.rejected();
        }

        @Override
        public Promise<PendingIntent> createKeyRetrievalIntent(CoreAccountInfo accountInfo) {
            Context context = InstrumentationRegistry.getContext();
            Intent intent = new Intent(context, DummyKeyRetrievalActivity.class);
            return Promise.fulfilled(
                    PendingIntent.getActivity(context, 0 /* requestCode */, intent, 0 /* flags */));
        }

        @Override
        public Promise<Boolean> markKeysAsStale(CoreAccountInfo accountInfo) {
            return Promise.rejected();
        }
    }

    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule() {
        @Override
        protected FakeProfileSyncService createProfileSyncService() {
            return new FakeProfileSyncService();
        }
    };

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    TrustedVaultClient.Natives mTrustedVaultClientNativesMock;

    private SettingsActivity mSettingsActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(TrustedVaultClientJni.TEST_HOOKS, mTrustedVaultClientNativesMock);
        // Register a mock native for calls to be forwarded to mTrustedVaultClientNativesMock.
        TrustedVaultClient.setInstanceForTesting(
                new TrustedVaultClient(new FakeTrustedVaultClientBackend()));
        TrustedVaultClient.registerNative(TRUSTED_VAULT_CLIENT_NATIVE_PTR);
    }

    @After
    public void tearDown() {
        TrustedVaultClient.unregisterNative(TRUSTED_VAULT_CLIENT_NATIVE_PTR);
        TrustedVaultClient.setInstanceForTesting(null);
    }

    /**
     * Test that triggering OnPassphraseAccepted dismisses PassphraseDialogFragment.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/986243")
    public void testPassphraseDialogDismissed() {
        final FakeProfileSyncService fakeProfileSyncService =
                (FakeProfileSyncService) mSyncTestRule.getProfileSyncService();

        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        // Trigger PassphraseDialogFragment to be shown when taping on Encryption.
        fakeProfileSyncService.setPassphraseRequiredForPreferredDataTypes(true);

        final ManageSyncSettings fragment = startManageSyncPreferences();
        Preference encryption = fragment.findPreference(ManageSyncSettings.PREF_ENCRYPTION);
        clickPreference(encryption);

        final PassphraseDialogFragment passphraseFragment = ActivityUtils.waitForFragment(
                mSettingsActivity, ManageSyncSettings.FRAGMENT_ENTER_PASSPHRASE);
        Assert.assertTrue(passphraseFragment.isAdded());

        // Simulate OnPassphraseAccepted from external event by setting PassphraseRequired to false
        // and triggering syncStateChanged().
        // PassphraseDialogFragment should be dismissed.
        fakeProfileSyncService.setPassphraseRequiredForPreferredDataTypes(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            fragment.getFragmentManager().executePendingTransactions();
            Assert.assertNull("PassphraseDialogFragment should be dismissed.",
                    mSettingsActivity.getFragmentManager().findFragmentByTag(
                            ManageSyncSettings.FRAGMENT_ENTER_PASSPHRASE));
        });
    }

    /**
     * Test the trusted vault key retrieval flow, which involves launching an intent and finally
     * calling TrustedVaultClient.notifyKeysChanged().
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testTrustedVaultKeyRetrieval() {
        final FakeProfileSyncService fakeProfileSyncService =
                (FakeProfileSyncService) mSyncTestRule.getProfileSyncService();

        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        fakeProfileSyncService.setEngineInitialized(true);
        fakeProfileSyncService.setTrustedVaultKeyRequired(true);

        final ManageSyncSettings fragment = startManageSyncPreferences();
        // Mimic the user tapping on Encryption.
        Preference encryption = fragment.findPreference(ManageSyncSettings.PREF_ENCRYPTION);
        clickPreference(encryption);

        // Wait for DummyKeyRetrievalActivity to finish.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Verify that notifyKeysChanged has been called upon completion.
        Mockito.verify(mTrustedVaultClientNativesMock)
                .notifyKeysChanged(TRUSTED_VAULT_CLIENT_NATIVE_PTR);
    }

    private ManageSyncSettings startManageSyncPreferences() {
        mSettingsActivity = mSyncTestRule.startSettingsActivity(ManageSyncSettings.class.getName());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (ManageSyncSettings) mSettingsActivity.getMainFragment();
    }

    private void clickPreference(final Preference pref) {
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> pref.getOnPreferenceClickListener().onPreferenceClick(pref));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}
