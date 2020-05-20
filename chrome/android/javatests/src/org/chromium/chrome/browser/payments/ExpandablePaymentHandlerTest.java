package org.chromium.chrome.browser.payments;

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerUiObserver;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.url.GURL;

/**
 * A test for the Expandable PaymentHandler {@link PaymentHandlerCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ExpandablePaymentHandlerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private boolean mUiShown = false;

    @Before
    public void setUp() {
        mRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testShow() throws Throwable {
        PaymentHandlerCoordinator paymentHandler = new PaymentHandlerCoordinator();
        PaymentHandlerUiObserver uiObserver = new PaymentHandlerUiObserver() {
            @Override
            public void onPaymentHandlerUiClosed() {}

            @Override
            public void onPaymentHandlerUiShown() {
                mUiShown = true;
            }
        };
        mRule.runOnUiThread(
                ()
                        -> paymentHandler.show(mRule.getActivity(),
                                new GURL("https://maxpay.com/pay"), /*isIncognito=*/false,
                                /*webContentsObserver=*/(webContents) -> {}, uiObserver));
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mUiShown;
            }
        });
    }
}
