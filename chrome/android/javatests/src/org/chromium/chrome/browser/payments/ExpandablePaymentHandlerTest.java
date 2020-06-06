// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.startsWith;

import android.os.RemoteException;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiSelector;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.util.concurrent.TimeoutException;

/**
 * A test for the Expandable PaymentHandler {@link PaymentHandlerCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + PaymentFeatureList.SCROLL_TO_EXPAND_PAYMENT_HANDLER})
public class ExpandablePaymentHandlerTest {
    private static final long MAX_WAIT_DURATION_MS = 3000L;

    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    // Open a tab on the blank page first to initiate the native bindings required by the test
    // server.
    @Rule
    public PaymentRequestTestRule mRule = new PaymentRequestTestRule("about:blank");

    // Host the tests on https://127.0.0.1, because file:// URLs cannot have service workers.
    private EmbeddedTestServer mServer;

    private UiDevice mDevice;

    @Before
    public void setUp() throws Throwable {
        mServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getContext(), ServerCertificate.CERT_OK);
        mRule.startMainActivityWithURL(
                mServer.getURL("/components/test/data/payments/maxpay.com/merchant.html"));

        // Find the web contents where JavaScript will be executed and instrument the browser
        // payment sheet.
        mRule.openPage();
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
    }

    private UiObject showPaymentHandlerUi() throws TimeoutException {
        mRule.clickNode("launch");
        UiObject bottomSheet = mDevice.findObject(
                new UiSelector().description("Payment handler sheet. Swipe down to close."));
        Assert.assertTrue(bottomSheet.waitForExists(MAX_WAIT_DURATION_MS));
        return bottomSheet;
    }

    private UiObject waitForTitleExists() {
        UiObject title = mDevice.findObject(new UiSelector().text("Max Pay"));
        Assert.assertTrue(title.waitForExists(MAX_WAIT_DURATION_MS));
        return title;
    }

    private void clickCloseButton() {
        onView(withId(org.chromium.chrome.R.id.close)).perform(click());
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testCloseButtonCloseUi() throws TimeoutException {
        UiObject bottomSheet = showPaymentHandlerUi();
        Assert.assertTrue(bottomSheet.exists());
        clickCloseButton();
        Assert.assertFalse(bottomSheet.exists());
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testSwipeDownCloseUi() throws TimeoutException, UiObjectNotFoundException {
        UiObject bottomSheet = showPaymentHandlerUi();
        UiObject title = waitForTitleExists();

        Assert.assertTrue(bottomSheet.exists());
        title.swipeDown(/*steps=*/10);
        Assert.assertFalse(bottomSheet.exists());
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testPressSystemBackAtFirstPageNotCloseUi() throws TimeoutException {
        UiObject bottomSheet = showPaymentHandlerUi();
        waitForTitleExists();

        Assert.assertTrue(bottomSheet.exists());
        Assert.assertFalse(mDevice.pressBack());
        mDevice.waitForIdle();

        Assert.assertTrue(bottomSheet.exists());
        clickCloseButton();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testOrientationChange() throws TimeoutException, RemoteException {
        UiObject bottomSheet = showPaymentHandlerUi();
        waitForTitleExists();
        Assert.assertTrue(bottomSheet.exists());

        mDevice.setOrientationLeft();
        mDevice.waitForIdle();
        Assert.assertTrue(bottomSheet.exists());

        clickCloseButton();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testWebContentsExist() throws TimeoutException {
        showPaymentHandlerUi();
        CriteriaHelper.pollUiThread(
                () -> PaymentRequestImpl.getPaymentHandlerWebContentsForTest() != null);
        clickCloseButton();
        CriteriaHelper.pollUiThread(
                () -> PaymentRequestImpl.getPaymentHandlerWebContentsForTest() == null);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testUiElementsExist() throws TimeoutException {
        showPaymentHandlerUi();
        waitForTitleExists();
        Assert.assertNotNull(PaymentRequestImpl.getPaymentHandlerWebContentsForTest());

        onView(withId(org.chromium.chrome.R.id.title))
                .check(matches(isDisplayed()))
                .check(matches(withText("Max Pay")));
        onView(withId(org.chromium.chrome.R.id.bottom_sheet))
                .check(matches(isDisplayed()))
                .check(matches(
                        withContentDescription("Payment handler sheet. Swipe down to close.")));
        onView(withId(org.chromium.chrome.R.id.close))
                .check(matches(isDisplayed()))
                .check(matches(withContentDescription("Close")));
        onView(withId(org.chromium.chrome.R.id.security_icon))
                .check(matches(isDisplayed()))
                .check(matches(withContentDescription("Connection is secure. Site information")));
        // Not verifying the port number because it's indefinite in tests.
        onView(withId(org.chromium.chrome.R.id.origin))
                .check(matches(isDisplayed()))
                .check(matches(withText(startsWith("127.0.0.1:"))));

        clickCloseButton();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testPageInfoButtonClickable() throws TimeoutException {
        UiObject bottomSheet = showPaymentHandlerUi();
        waitForTitleExists();

        onView(withId(org.chromium.chrome.R.id.security_icon)).perform(click());

        String paymentAppUrl = mServer.getURL(
                "/components/test/data/payments/maxpay.com/payment_handler_window.html");
        onView(withId(org.chromium.chrome.R.id.page_info_url))
                .check(matches(isDisplayed()))
                .check(matches(withText(paymentAppUrl)));

        Assert.assertFalse(bottomSheet.exists());
        mDevice.pressBack();
        Assert.assertTrue(bottomSheet.waitForExists(MAX_WAIT_DURATION_MS));
        clickCloseButton();
    }
}
