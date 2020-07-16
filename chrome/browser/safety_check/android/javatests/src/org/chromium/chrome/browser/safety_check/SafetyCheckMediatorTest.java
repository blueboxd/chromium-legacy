// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.os.Handler;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.password_check.BulkLeakCheckServiceState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.PasswordsState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Unit tests for {@link SafetyCheckMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyCheckMediatorTest {
    private PropertyModel mModel;
    @Mock
    private SafetyCheckUpdatesDelegate mUpdatesDelegate;
    @Mock
    private SafetyCheckBridge mBridge;
    @Mock
    private Handler mHandler;

    private SafetyCheckMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = SafetyCheckProperties.createSafetyCheckModel();
        mMediator = new SafetyCheckMediator(mModel, mUpdatesDelegate, mBridge, mHandler);
        // Execute any delayed tasks immediately.
        doAnswer(invocation -> {
            Runnable runnable = (Runnable) (invocation.getArguments()[0]);
            runnable.run();
            return null;
        })
                .when(mHandler)
                .postDelayed(any(Runnable.class), anyLong());
    }

    @Test
    public void testUpdatesCheckUpdated() {
        doAnswer(invocation -> {
            Callback<Integer> callback =
                    ((WeakReference<Callback<Integer>>) invocation.getArguments()[0]).get();
            callback.onResult(UpdatesState.UPDATED);
            return null;
        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));

        mMediator.performSafetyCheck();
        assertEquals(UpdatesState.UPDATED, mModel.get(SafetyCheckProperties.UPDATES_STATE));
    }

    @Test
    public void testUpdatesCheckOutdated() {
        doAnswer(invocation -> {
            Callback<Integer> callback =
                    ((WeakReference<Callback<Integer>>) invocation.getArguments()[0]).get();
            callback.onResult(UpdatesState.OUTDATED);
            return null;
        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));

        mMediator.performSafetyCheck();
        assertEquals(UpdatesState.OUTDATED, mModel.get(SafetyCheckProperties.UPDATES_STATE));
    }

    @Test
    public void testSafeBrowsingCheckEnabledStandard() {
        doAnswer(invocation -> {
            mMediator.onSafeBrowsingCheckResult(SafeBrowsingStatus.ENABLED_STANDARD);
            return null;
        })
                .when(mBridge)
                .checkSafeBrowsing();

        mMediator.performSafetyCheck();
        assertEquals(SafeBrowsingState.ENABLED_STANDARD,
                mModel.get(SafetyCheckProperties.SAFE_BROWSING_STATE));
    }

    @Test
    public void testSafeBrowsingCheckDisabled() {
        doAnswer(invocation -> {
            mMediator.onSafeBrowsingCheckResult(SafeBrowsingStatus.DISABLED);
            return null;
        })
                .when(mBridge)
                .checkSafeBrowsing();

        mMediator.performSafetyCheck();
        assertEquals(
                SafeBrowsingState.DISABLED, mModel.get(SafetyCheckProperties.SAFE_BROWSING_STATE));
    }

    @Test
    public void testPasswordsCheckError() {
        doAnswer(invocation -> {
            mMediator.onPasswordCheckStateChange(BulkLeakCheckServiceState.SERVICE_ERROR);
            return null;
        })
                .when(mBridge)
                .checkPasswords();

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.ERROR, mModel.get(SafetyCheckProperties.PASSWORDS_STATE));
    }

    @Test
    public void testPasswordsCheckNoPasswords() {
        doAnswer(invocation -> {
            mMediator.onPasswordCheckStateChange(BulkLeakCheckServiceState.IDLE);
            return null;
        })
                .when(mBridge)
                .checkPasswords();
        when(mBridge.savedPasswordsExist()).thenReturn(false);

        mMediator.performSafetyCheck();
        assertEquals(
                PasswordsState.NO_PASSWORDS, mModel.get(SafetyCheckProperties.PASSWORDS_STATE));
    }

    @Test
    public void testPasswordsCheckNoLeaks() {
        doAnswer(invocation -> {
            mMediator.onPasswordCheckStateChange(BulkLeakCheckServiceState.IDLE);
            return null;
        })
                .when(mBridge)
                .checkPasswords();
        when(mBridge.savedPasswordsExist()).thenReturn(true);
        when(mBridge.getNumberOfPasswordLeaksFromLastCheck()).thenReturn(0);

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.SAFE, mModel.get(SafetyCheckProperties.PASSWORDS_STATE));
    }

    @Test
    public void testPasswordsCheckHasLeaks() {
        doAnswer(invocation -> {
            mMediator.onPasswordCheckStateChange(BulkLeakCheckServiceState.IDLE);
            return null;
        })
                .when(mBridge)
                .checkPasswords();
        when(mBridge.savedPasswordsExist()).thenReturn(true);
        when(mBridge.getNumberOfPasswordLeaksFromLastCheck()).thenReturn(123);

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.COMPROMISED_EXIST,
                mModel.get(SafetyCheckProperties.PASSWORDS_STATE));
    }
}
