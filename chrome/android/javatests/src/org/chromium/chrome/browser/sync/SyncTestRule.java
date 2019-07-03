// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.content.Context;
import android.support.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.identity.UniqueIdentificationGenerator;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SignoutReason;
import org.chromium.chrome.browser.signin.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * TestRule for common functionality between sync tests.
 */
public class SyncTestRule extends ChromeActivityTestRule<ChromeActivity> {
    private static final String TAG = "SyncTestBase";

    private static final String CLIENT_ID = "Client_ID";

    private static final Set<Integer> USER_SELECTABLE_TYPES =
            new HashSet<Integer>(Arrays.asList(new Integer[] {
                    ModelType.AUTOFILL, ModelType.BOOKMARKS, ModelType.PASSWORDS,
                    ModelType.PREFERENCES, ModelType.PROXY_TABS, ModelType.TYPED_URLS,
            }));

    public abstract static class DataCriteria<T> extends Criteria {
        public DataCriteria() {
            super("Sync data criteria not met.");
        }

        public abstract boolean isSatisfied(List<T> data);

        public abstract List<T> getData() throws Exception;

        @Override
        public boolean isSatisfied() {
            try {
                return isSatisfied(getData());
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    private Context mContext;
    private FakeServerHelper mFakeServerHelper;
    private ProfileSyncService mProfileSyncService;
    private MockSyncContentResolverDelegate mSyncContentResolver;

    public SyncTestRule() {
        super(ChromeActivity.class);
    }

    /**Getters for Test variables */
    public Context getTargetContext() {
        return mContext;
    }

    public FakeServerHelper getFakeServerHelper() {
        return mFakeServerHelper;
    }

    public ProfileSyncService getProfileSyncService() {
        return mProfileSyncService;
    }

    public MockSyncContentResolverDelegate getSyncContentResolver() {
        return mSyncContentResolver;
    }

    public void startMainActivityForSyncTest() throws Exception {
        // Start the activity by opening about:blank. This URL is ideal because it is not synced as
        // a typed URL. If another URL is used, it could interfere with test data.
        startMainActivityOnBlankPage();
    }

    private void setUpMockAndroidSyncSettings() {
        mSyncContentResolver = new MockSyncContentResolverDelegate();
        mSyncContentResolver.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(mSyncContentResolver, null);
    }

    public Account setUpTestAccount() {
        Account account = SigninTestUtil.addTestAccount();
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        return account;
    }

    public Account setUpTestAccountAndSignIn() {
        Account account = setUpTestAccount();
        signIn(account);
        return account;
    }

    public void startSync() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mProfileSyncService.requestStart(); });
    }

    public void startSyncAndWait() {
        startSync();
        SyncTestUtil.waitForSyncActive();
    }

    public void stopSync() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mProfileSyncService.requestStop(); });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    public void signIn(final Account account) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SigninManager.get().signIn(account, null, null);
            // Outside of tests, URL-keyed anonymized data collection is enabled by sign-in UI.
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(true);
        });
        SyncTestUtil.waitForSyncActive();
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        Assert.assertEquals(account, SigninTestUtil.getCurrentAccount());
    }

    public void signOut() throws InterruptedException {
        final Semaphore s = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SigninManager.get().signOut(SignoutReason.SIGNOUT_TEST, new Runnable() {
                @Override
                public void run() {
                    s.release();
                }
            });
        });
        Assert.assertTrue(s.tryAcquire(SyncTestUtil.TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertNull(SigninTestUtil.getCurrentAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
    }

    public void clearServerData() {
        mFakeServerHelper.clearServerData();
        SyncTestUtil.triggerSync();
        CriteriaHelper.pollUiThread(new Criteria("Timed out waiting for sync to stop.") {
            @Override
            public boolean isSatisfied() {
                return !ProfileSyncService.get().isSyncRequested();
            }
        }, SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    /*
     * Enables the |modelType| Sync data type, which must be in USER_SELECTABLE_TYPES.
     */
    public void enableDataType(final int modelType) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Set<Integer> chosenTypes = mProfileSyncService.getChosenDataTypes();
            chosenTypes.add(modelType);
            mProfileSyncService.setChosenDataTypes(false, chosenTypes);
        });
    }

    /*
     * Disables the |modelType| Sync data type, which must be in USER_SELECTABLE_TYPES.
     */
    public void disableDataType(final int modelType) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Set<Integer> chosenTypes = mProfileSyncService.getChosenDataTypes();
            chosenTypes.remove(modelType);
            mProfileSyncService.setChosenDataTypes(false, chosenTypes);
        });
    }

    public void pollInstrumentationThread(Criteria criteria) {
        CriteriaHelper.pollInstrumentationThread(
                criteria, SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    @Override
    public Statement apply(final Statement statement, final Description desc) {
        final Statement base = super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUpMockAndroidSyncSettings();

                startMainActivityForSyncTest();
                mContext = InstrumentationRegistry.getTargetContext();

                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    // Ensure SyncController is registered with the new AndroidSyncSettings.
                    AndroidSyncSettings.get().registerObserver(SyncController.get(mContext));
                    mFakeServerHelper = FakeServerHelper.get();
                });
                FakeServerHelper.useFakeServer(mContext);
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> { mProfileSyncService = ProfileSyncService.get(); });

                UniqueIdentificationGeneratorFactory.registerGenerator(
                        UuidBasedUniqueIdentificationGenerator.GENERATOR_ID,
                        new UniqueIdentificationGenerator() {
                            @Override
                            public String getUniqueId(String salt) {
                                return CLIENT_ID;
                            }
                        },
                        true);
                statement.evaluate();
            }
        }, desc);
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                ruleSetUp();
                base.evaluate();
                ruleTearDown();
            }
        };
    }

    private void ruleSetUp() throws Throwable {
        // This must be called before super.setUp() in order for test authentication to work.
        SigninTestUtil.setUpAuthForTest();
    }

    private void ruleTearDown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProfileSyncService.requestStop();
            FakeServerHelper.deleteFakeServer();
        });
        SigninTestUtil.tearDownAuthForTest();
    }
}
