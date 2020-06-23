// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.password_check.BulkLeakCheckServiceState;

/** Unit tests for {@link SafetyCheckModel}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyCheckModelTest {
    @Mock
    private SafetyCheckSettingsFragment mView;

    private SafetyCheckModel mSafetyCheckModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mSafetyCheckModel = new SafetyCheckModel(mView);
    }

    /**
     * Tests a standard workflow for Safe Browsing.
     */
    @Test
    public void testSafeBrowsing() {
        mSafetyCheckModel.setCheckingState();
        verify(mView).updateElementStatus("safe_browsing", R.string.safety_check_checking);
        mSafetyCheckModel.updateSafeBrowsingStatus(SafeBrowsingStatus.ENABLED_STANDARD);
        verify(mView).updateElementStatus(
                "safe_browsing", R.string.safety_check_safe_browsing_enabled_standard);
    }

    /**
     * Tests a successful workflow for the passwords check.
     */
    @Test
    public void testPasswordsSuccess() {
        mSafetyCheckModel.setCheckingState();
        verify(mView).updateElementStatus("passwords", R.string.safety_check_checking);
        mSafetyCheckModel.updatePasswordsStatusOnSucess(false, 0);
        verify(mView).updateElementStatus(
                "passwords", R.string.safety_check_passwords_no_passwords);
    }

    /**
     * Tests a failure workflow for the passwords check.
     */
    @Test
    public void testPasswordsError() {
        mSafetyCheckModel.setCheckingState();
        verify(mView).updateElementStatus("passwords", R.string.safety_check_checking);
        mSafetyCheckModel.updatePasswordsStatusOnError(
                BulkLeakCheckServiceState.TOKEN_REQUEST_FAILURE);
        verify(mView).updateElementStatus("passwords", R.string.safety_check_error);
    }
}
