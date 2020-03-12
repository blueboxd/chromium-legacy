// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.content.Intent;
import android.os.Looper;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.intent.IsReadyToPayServiceHelper;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

/**
 * Tests for IsReadyToPayServiceHelper.
 **/
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IsReadyToPayServiceHelperTest implements IsReadyToPayServiceHelper.ResultHandler {
    private Context mContext;
    private boolean mErrorReceived;
    private IsReadyToPayServiceHelper mHelper;

    @Override
    public void onIsReadyToPayServiceResponse(boolean isReadyToPay) {}

    @Override
    public void onIsReadyToPayServiceError() {}

    @Before
    public void setUp() throws Throwable {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void tearDown() throws Throwable {}

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void smokeTest() throws Throwable {
        Looper.prepare();
        Intent intent = new Intent();
        intent.setClassName("any.package.name", "any.service.name");
        IsReadyToPayServiceHelper helper =
                new IsReadyToPayServiceHelper(mContext, intent, /*resultHandler=*/this);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void onIsReadyToPayServiceErrorTest() throws Throwable {
        Looper.prepare();
        mErrorReceived = false;
        Intent intent = new Intent();
        intent.setClassName("any.package.name", "any.service.name");
        mHelper = new IsReadyToPayServiceHelper(
                mContext, intent, new IsReadyToPayServiceHelper.ResultHandler() {
                    @Override
                    public void onIsReadyToPayServiceResponse(boolean isReadyToPay) {
                        Assert.fail();
                    }
                    @Override
                    public void onIsReadyToPayServiceError() {
                        mErrorReceived = true;
                    }
                });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mErrorReceived;
            }
        });
    }
}
