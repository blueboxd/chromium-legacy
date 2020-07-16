// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

/**
 * Tests EnterpriseInfo.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class EnterpriseInfoTest {
    @Before
    public void setUp() {
        EnterpriseInfo.reset();
        // Skip the AsyncTask, we don't actually want to query the device, just enqueue callbacks.
        EnterpriseInfo.getInstance().setSkipAsyncCheckForTesting(true);
    }

    /**
     * Tests that the callback is called with the correct result.
     * Tests both the first computation and the cached value.
     */
    @Test
    @SmallTest
    public void testCallbacksGetResultValue() {
        EnterpriseInfo instance = EnterpriseInfo.getInstance();

        EnterpriseInfo.OwnedState stateIn = new EnterpriseInfo.OwnedState(false, true);

        class CallbackWithResult implements Callback<EnterpriseInfo.OwnedState> {
            public EnterpriseInfo.OwnedState result;

            @Override
            public void onResult(EnterpriseInfo.OwnedState result) {
                this.result = result;
            }
        };
        CallbackWithResult callback = new CallbackWithResult();
        CallbackWithResult callback2 = new CallbackWithResult();

        // Make the request and service the callbacks.
        instance.getDeviceEnterpriseInfo(callback);
        instance.getDeviceEnterpriseInfo(callback2);
        instance.onEnterpriseInfoResult(stateIn);

        // Results should be the same for all callbacks.
        Assert.assertEquals("Callback doesn't match the expected result on servicing.",
                callback.result, stateIn);
        Assert.assertEquals("Callback doesn't match the expected result on servicing.",
                callback2.result, stateIn);

        // Reset the callbacks.
        callback.result = null;
        callback2.result = null;
        Assert.assertNotEquals("Callback wasn't reset properly.", callback.result, stateIn);
        Assert.assertNotEquals("Callback wasn't reset properly.", callback2.result, stateIn);

        // Check the cached value is returned correctly.
        instance.getDeviceEnterpriseInfo(callback);
        instance.getDeviceEnterpriseInfo(callback2);
        // This should happen immediately, see testCallbackServicedWhenResultCached.
        Assert.assertEquals(
                "Callback doesn't match the expected cached result.", callback.result, stateIn);
        Assert.assertEquals(
                "Callback doesn't match the expected cached result.", callback2.result, stateIn);
    }

    /**
     * Test that if multiple callbacks get queued up that they're all serviced.
     */
    @Test
    @SmallTest
    public void testMultipleCallbacksServiced() {
        EnterpriseInfo instance = EnterpriseInfo.getInstance();
        CallbackHelper helper = new CallbackHelper();

        Callback<EnterpriseInfo.OwnedState> callback = (result) -> {
            // We don't care about the result in this test.
            helper.notifyCalled();
        };

        // Load up requests
        final int count = 5;
        for (int i = 0; i < count; i++) {
            instance.getDeviceEnterpriseInfo(callback);
        }

        // Nothing should be called yet.
        Assert.assertEquals(
                "Callbacks were serviced before they were meant to be.", 0, helper.getCallCount());

        // Do it. The result value here is irrelevant, put anything.
        instance.onEnterpriseInfoResult(new EnterpriseInfo.OwnedState(true, true));

        Assert.assertEquals(
                "The wrong number of callbacks were serviced.", count, helper.getCallCount());
    }

    /**
     * Tests that a callback is only serviced immediately if there is a cached result.
     */
    @Test
    @SmallTest
    public void testCallbackServicedWhenResultCached() {
        EnterpriseInfo instance = EnterpriseInfo.getInstance();
        CallbackHelper helper = new CallbackHelper();

        Callback<EnterpriseInfo.OwnedState> callback = (result) -> {
            // We don't care about the result in this test.
            helper.notifyCalled();
        };

        // First: Callback should not be serviced if nothing is cached.
        instance.getDeviceEnterpriseInfo(callback);
        Assert.assertEquals(
                "Callbacks were serviced before they were meant to be.", 0, helper.getCallCount());

        // Set a cached result (of any value) and process the callback.
        instance.onEnterpriseInfoResult(new EnterpriseInfo.OwnedState(false, false));
        Assert.assertEquals("Only a single callback should have been serviced at this point.", 1,
                helper.getCallCount());

        // Second: Callback should now be serviced immediately.
        instance.getDeviceEnterpriseInfo(callback);
        Assert.assertEquals(
                "Callback wasn't serviced by the cached result.", 2, helper.getCallCount());
    }

    /**
     * Tests that OwnedStates's overridden equals() works as expected.
     */
    @Test
    @SmallTest
    public void testOwnedStateEquals() {
        // Two references to the same object are equal. Values don't matter here.
        EnterpriseInfo.OwnedState ref = new EnterpriseInfo.OwnedState(true, true);
        EnterpriseInfo.OwnedState sameRef = ref;
        Assert.assertEquals("Same reference check failed.", ref, sameRef);

        // This is also true for null.
        EnterpriseInfo.OwnedState nullRef = null;
        EnterpriseInfo.OwnedState nullRef2 = null;
        Assert.assertEquals("Null reference check failed.", nullRef, nullRef2);

        // A valid object and null should not be equal.

        Assert.assertNotEquals("Valid obj == null check failed.", ref, nullRef);

        // A different type of, valid, object shouldn't be equal.
        Object obj = new Object();
        Assert.assertNotEquals("Wrong object type check failed.", ref, obj);

        // Two valid owned states should only be equal when their member variables are all the same.
        EnterpriseInfo.OwnedState trueTrue = ref;
        EnterpriseInfo.OwnedState falseFalse = new EnterpriseInfo.OwnedState(false, false);
        EnterpriseInfo.OwnedState trueFalse = new EnterpriseInfo.OwnedState(true, false);
        EnterpriseInfo.OwnedState falseTrue = new EnterpriseInfo.OwnedState(false, true);
        EnterpriseInfo.OwnedState trueTrue2 = new EnterpriseInfo.OwnedState(true, true);

        Assert.assertNotEquals("Wrong value check failed.", trueTrue, falseFalse);
        Assert.assertNotEquals("Wrong value check failed.", trueTrue, trueFalse);
        Assert.assertNotEquals("Wrong value check failed.", trueTrue, falseTrue);
        Assert.assertEquals("Correct value check failed.", trueTrue, trueTrue2);
    }
}
