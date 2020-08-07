// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.safety_check.SafetyCheckBridge.SafetyCheckCommonObserver;

/** Unit tests for {@link SafetyCheckBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyCheckBridgeTest {
    class TestObserver implements SafetyCheckCommonObserver {
        public boolean sbCallbackInvoked;

        private @SafeBrowsingStatus int mSBExpected = -1;

        @Override
        public void onSafeBrowsingCheckResult(@SafeBrowsingStatus int status) {
            sbCallbackInvoked = true;
            assertEquals(mSBExpected, status);
        }

        public void setSafeBrowsingExpected(@SafeBrowsingStatus int expected) {
            mSBExpected = expected;
        }
    }

    @Rule
    public JniMocker mocker = new JniMocker();
    @Mock
    private SafetyCheckBridge.Natives mNativeMock;

    private SafetyCheckBridge mSafetyCheckBridge;
    private TestObserver mObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(SafetyCheckBridgeJni.TEST_HOOKS, mNativeMock);
        mObserver = new TestObserver();
        mSafetyCheckBridge = new SafetyCheckBridge(mObserver);
    }

    @Test
    public void testCheckSafeBrowsing() {
        assertFalse(mObserver.sbCallbackInvoked);

        @SafeBrowsingStatus
        int expected = SafeBrowsingStatus.DISABLED_BY_ADMIN;
        doAnswer(invocation -> {
            mObserver.onSafeBrowsingCheckResult(expected);
            return null;
        })
                .when(mNativeMock)
                .checkSafeBrowsing(anyLong(), any(SafetyCheckBridge.class));
        mObserver.setSafeBrowsingExpected(expected);

        mSafetyCheckBridge.checkSafeBrowsing();

        assertTrue(mObserver.sbCallbackInvoked);
    }
}
