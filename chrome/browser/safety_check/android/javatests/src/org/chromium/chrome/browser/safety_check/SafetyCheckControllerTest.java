// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.safety_check.SafetyCheckModel.Updates;

/** Unit tests for {@link SafetyCheckModel}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyCheckControllerTest {
    @Mock
    private SafetyCheckModel mModel;
    @Mock
    private SafetyCheckUpdatesDelegate mUpdatesDelegate;
    @Mock
    private SafetyCheckBridge mBridge;

    private SafetyCheckController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mController = new SafetyCheckController(mModel, mUpdatesDelegate, mBridge);
    }

    /** Tests that the updates callback is routed correctly. */
    @Test
    public void testUpdatesCheck() {
        doAnswer(invocation -> {
            Callback<Updates> callback = (Callback<Updates>) invocation.getArguments()[0];
            callback.onResult(Updates.UPDATED);
            return null;
        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(Callback.class));

        mController.performSafetyCheck();
        // The model should get updated with the result of the callback.
        verify(mModel).updateUpdatesStatus(Updates.UPDATED);
    }
}
