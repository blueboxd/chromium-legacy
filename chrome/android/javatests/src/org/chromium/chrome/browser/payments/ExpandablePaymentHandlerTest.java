// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.swipeDown;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.startsWith;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.uiautomator.UiDevice;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerUiObserver;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerWebContentsObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.url.GURL;

/**
 * A test for the Expandable PaymentHandler {@link PaymentHandlerCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ExpandablePaymentHandlerTest {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    // Open a tab on the blank page first to initiate the native bindings required by the test
    // server.
    @Rule
    public PaymentRequestTestRule mRule = new PaymentRequestTestRule("about:blank");

    // Host the tests on https://127.0.0.1, because file:// URLs cannot have service workers.
    private EmbeddedTestServer mServer;
    private GURL mDefaultPaymentAppUrl;
    private boolean mUiShownCalled = false;
    private boolean mUiClosedCalled = false;
    private boolean mWebContentsInitializedCallbackInvoked = false;
    private UiDevice mDevice;
    private boolean mDefaultIsIncognito = false;
    private ChromeActivity mDefaultActivity;

    @Before
    public void setUp() throws Throwable {
        mServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getContext(), ServerCertificate.CERT_OK);
        mDefaultPaymentAppUrl = new GURL(mServer.getURL(
                "/components/test/data/payments/maxpay.com/payment_handler_window.html"));
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mDefaultActivity = mRule.getActivity();
    }

    private PaymentHandlerWebContentsObserver defaultWebContentObserver() {
        return new PaymentHandlerWebContentsObserver() {
            @Override
            public void onWebContentsInitialized(WebContents webContents) {
                mWebContentsInitializedCallbackInvoked = true;
            }
        };
    }

    private PaymentHandlerUiObserver defaultUiObserver() {
        return new PaymentHandlerUiObserver() {
            @Override
            public void onPaymentHandlerUiClosed() {
                mUiClosedCalled = true;
            }

            @Override
            public void onPaymentHandlerUiShown() {
                mUiShownCalled = true;
            }
        };
    }

    private void waitForUiShown() {
        CriteriaHelper.pollInstrumentationThread(() -> mUiShownCalled);
    }

    private void waitForTitleShown(WebContents paymentAppWebContents) {
        CriteriaHelper.pollInstrumentationThread(
                () -> paymentAppWebContents.getTitle().equals("Max Pay"));
    }

    private void waitForUiClosed() {
        CriteriaHelper.pollInstrumentationThread(() -> mUiClosedCalled);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testOpenClose() throws Throwable {
        PaymentHandlerCoordinator paymentHandler = new PaymentHandlerCoordinator();
        mRule.runOnUiThread(()
                                    -> paymentHandler.show(mDefaultActivity, mDefaultPaymentAppUrl,
                                            /*isIncognito=*/true, defaultWebContentObserver(),
                                            defaultUiObserver()));
        waitForUiShown();

        mRule.runOnUiThread(() -> paymentHandler.hide());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testSwipeDownCloseUI() throws Throwable {
        PaymentHandlerCoordinator paymentHandler = new PaymentHandlerCoordinator();
        mRule.runOnUiThread(()
                                    -> paymentHandler.show(mDefaultActivity, mDefaultPaymentAppUrl,
                                            mDefaultIsIncognito, defaultWebContentObserver(),
                                            defaultUiObserver()));
        waitForUiShown();

        onView(withId(R.id.bottom_sheet_control_container)).perform(swipeDown());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testClickCloseButtonCloseUI() throws Throwable {
        PaymentHandlerCoordinator paymentHandler = new PaymentHandlerCoordinator();
        mRule.runOnUiThread(()
                                    -> paymentHandler.show(mDefaultActivity, mDefaultPaymentAppUrl,
                                            mDefaultIsIncognito, defaultWebContentObserver(),
                                            defaultUiObserver()));
        waitForUiShown();

        onView(withId(R.id.close)).perform(click());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testWebContentsInitializedCallbackInvoked() throws Throwable {
        PaymentHandlerCoordinator paymentHandler = new PaymentHandlerCoordinator();
        mRule.runOnUiThread(()
                                    -> paymentHandler.show(mDefaultActivity, mDefaultPaymentAppUrl,
                                            mDefaultIsIncognito, defaultWebContentObserver(),
                                            defaultUiObserver()));
        waitForUiShown();

        Assert.assertTrue(mWebContentsInitializedCallbackInvoked);

        mRule.runOnUiThread(() -> paymentHandler.hide());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testWebContentsDestroy() throws Throwable {
        PaymentHandlerCoordinator paymentHandler = new PaymentHandlerCoordinator();
        mRule.runOnUiThread(()
                                    -> paymentHandler.show(mDefaultActivity, mDefaultPaymentAppUrl,
                                            mDefaultIsIncognito, defaultWebContentObserver(),
                                            defaultUiObserver()));
        waitForUiShown();

        Assert.assertFalse(paymentHandler.getWebContentsForTest().isDestroyed());
        mRule.runOnUiThread(() -> paymentHandler.hide());
        waitForUiClosed();
        Assert.assertTrue(paymentHandler.getWebContentsForTest().isDestroyed());
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testIncognitoTrue() throws Throwable {
        PaymentHandlerCoordinator paymentHandler = new PaymentHandlerCoordinator();
        mRule.runOnUiThread(()
                                    -> paymentHandler.show(mDefaultActivity, mDefaultPaymentAppUrl,
                                            /*isIncognito=*/true, defaultWebContentObserver(),
                                            defaultUiObserver()));
        waitForUiShown();

        Assert.assertTrue(paymentHandler.getWebContentsForTest().isIncognito());

        mRule.runOnUiThread(() -> paymentHandler.hide());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testIncognitoFalse() throws Throwable {
        PaymentHandlerCoordinator paymentHandler = new PaymentHandlerCoordinator();
        mRule.runOnUiThread(()
                                    -> paymentHandler.show(mDefaultActivity, mDefaultPaymentAppUrl,
                                            /*isIncognito=*/false, defaultWebContentObserver(),
                                            defaultUiObserver()));
        waitForUiShown();

        Assert.assertFalse(paymentHandler.getWebContentsForTest().isIncognito());

        mRule.runOnUiThread(() -> paymentHandler.hide());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testUiElements() throws Throwable {
        PaymentHandlerCoordinator paymentHandler = new PaymentHandlerCoordinator();
        mRule.runOnUiThread(()
                                    -> paymentHandler.show(mDefaultActivity, mDefaultPaymentAppUrl,
                                            mDefaultIsIncognito, defaultWebContentObserver(),
                                            defaultUiObserver()));
        waitForUiShown();

        onView(withId(R.id.bottom_sheet))
                .check(matches(
                        withContentDescription("Payment handler sheet. Swipe down to close.")));

        CriteriaHelper.pollInstrumentationThread(
                () -> paymentHandler.getWebContentsForTest().getTitle().equals("Max Pay"));

        onView(withId(R.id.title))
                .check(matches(isDisplayed()))
                .check(matches(withText("Max Pay")));
        onView(withId(R.id.bottom_sheet))
                .check(matches(isDisplayed()))
                .check(matches(
                        withContentDescription("Payment handler sheet. Swipe down to close.")));
        onView(withId(R.id.close))
                .check(matches(isDisplayed()))
                .check(matches(withContentDescription("Close")));
        onView(withId(R.id.security_icon))
                .check(matches(isDisplayed()))
                .check(matches(withContentDescription("Connection is secure. Site information")));
        // Not verifying the port number because it's indefinite in tests.
        onView(withId(R.id.origin))
                .check(matches(isDisplayed()))
                .check(matches(withText(startsWith("127.0.0.1:"))));

        mRule.runOnUiThread(() -> paymentHandler.hide());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testOpenPageInfoDialog() throws Throwable {
        PaymentHandlerCoordinator paymentHandler = new PaymentHandlerCoordinator();
        mRule.runOnUiThread(()
                                    -> paymentHandler.show(mDefaultActivity, mDefaultPaymentAppUrl,
                                            mDefaultIsIncognito, defaultWebContentObserver(),
                                            defaultUiObserver()));
        waitForTitleShown(paymentHandler.getWebContentsForTest());

        onView(withId(R.id.security_icon)).perform(click());

        String paymentAppUrl = mServer.getURL(
                "/components/test/data/payments/maxpay.com/payment_handler_window.html");
        onView(withId(R.id.page_info_url))
                .check(matches(isDisplayed()))
                .check(matches(withText(paymentAppUrl)));

        mDevice.pressBack();

        onView(withId(R.id.page_info_url)).check(doesNotExist());

        mRule.runOnUiThread(() -> paymentHandler.hide());
        waitForUiClosed();
    }
}
