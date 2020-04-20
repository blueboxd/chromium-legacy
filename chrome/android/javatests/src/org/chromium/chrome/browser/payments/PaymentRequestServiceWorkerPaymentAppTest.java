// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.BasicCardNetwork;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for service worker based payment apps.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        // For all the tests in this file, we expect abort exception when there is no supported
        // payment apps instead of showing payment request UI.
        "enable-features=" + ChromeFeatureList.STRICT_HAS_ENROLLED_AUTOFILL_INSTRUMENT,
        // Prevent crawling the web for real payment apps.
        "disable-features=" + ChromeFeatureList.SERVICE_WORKER_PAYMENT_APPS + ","
                + ChromeFeatureList.SCROLL_TO_EXPAND_PAYMENT_HANDLER})
public class PaymentRequestServiceWorkerPaymentAppTest {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule = new PaymentRequestTestRule(
            "payment_request_bobpay_and_basic_card_with_modifier_optional_data_test.html");

    /**
     * Installs a mock service worker based payment app with given supported delegations for
     * testing.
     *
     * @param scope                Service worker scope that identifies the payment app. Must be
     *                             unique.
     * @param supportedMethodNames The supported payment methods of the mock payment app.
     * @param capabilities         The capabilities of the mocked payment app.
     * @param name                 The name of the mocked payment app.
     * @param withIcon             Whether provide payment app icon.
     * @param supportedDelegations The supported delegations of the mock payment app.
     */
    private void installMockServiceWorkerPaymentApp(String scope,
            final String[] supportedMethodNames,
            final ServiceWorkerPaymentApp.Capabilities[] capabilities, final String name,
            final boolean withIcon, SupportedDelegations supportedDelegations) {
        PaymentAppService.getInstance().addFactory(new PaymentAppFactoryInterface() {
            @Override
            public void create(PaymentAppFactoryDelegate delegate) {
                WebContents webContents = delegate.getParams().getWebContents();
                ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
                BitmapDrawable icon = withIcon
                        ? new BitmapDrawable(activity.getResources(),
                                Bitmap.createBitmap(new int[] {Color.RED}, 1 /* width */,
                                        1 /* height */, Bitmap.Config.ARGB_8888))
                        : null;
                delegate.onPaymentAppCreated(new ServiceWorkerPaymentApp(webContents,
                        0 /* registrationId */, new GURL(scope) /*scope*/, name,
                        "test@bobpay.com" /* userHint */, icon /* icon */,
                        supportedMethodNames /* methodNames */, capabilities /* capabilities */,
                        new String[0] /* preferredRelatedApplicationIds */, supportedDelegations));
                delegate.onDoneCreatingPaymentApps(this);
            }
        });
    }

    /**
     * Installs a mock service worker based payment app with no supported delegations for testing.
     *
     * @param scope                The service worker scope that identifies this payment app. Must
     *                             be unique.
     * @param supportedMethodNames The supported payment methods of the mock payment app.
     * @param capabilities         The capabilities of the mocked payment app.
     * @param withName             Whether provide payment app name.
     * @param withIcon             Whether provide payment app icon.
     */
    private void installMockServiceWorkerPaymentApp(String scope,
            final String[] supportedMethodNames,
            final ServiceWorkerPaymentApp.Capabilities[] capabilities, final boolean withName,
            final boolean withIcon) {
        installMockServiceWorkerPaymentApp(scope, supportedMethodNames, capabilities,
                withName ? "BobPay" : null, withIcon, new SupportedDelegations());
    }

    /**
     * Installs a mock service worker based payment app for bobpay with given supported delegations
     * for testing.
     *
     * @param scope             The service worker scope that identifies this payment app. Must be
     *                          unique.
     * @param shippingAddress   Whether or not the mock payment app provides shipping address.
     * @param payerName         Whether or not the mock payment app provides payer's name.
     * @param payerPhone        Whether or not the mock payment app provides payer's phone number.
     * @param payerEmail        Whether or not the mock payment app provides payer's email address.
     * @param name              The name of the mocked payment app.
     */
    private void installMockServiceWorkerPaymentAppWithDelegations(String scope,
            final boolean shippingAddress, final boolean payerName, final boolean payerPhone,
            final boolean payerEmail, final String name) {
        String[] supportedMethodNames = {"https://bobpay.xyz"};
        installMockServiceWorkerPaymentApp(scope, supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], name, true /*withIcon*/,
                new SupportedDelegations(shippingAddress, payerName, payerPhone, payerEmail));
    }

    /**
     * Adds a credit cart to ensure that autofill app is available.
     */
    public void addCreditCard() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "John Smith", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "310-310-6000", "john.smith@gmail.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                billingAddressId, "" /* serverId */));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoSupportedPaymentMethods() throws TimeoutException {
        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "The payment method", "not supported"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasSupportedPaymentMethods() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp("https://bobpay.com", supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], true, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);
        // Payment sheet skips to the app since it is the only available app.
        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getDismissed());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoCapabilities() throws TimeoutException {
        // Add a credit card to force showing payment sheet UI.
        addCreditCard();

        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {};
        installMockServiceWorkerPaymentApp(
                "https://bobpay.com", supportedMethodNames, capabilities, true, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfPaymentApps());
        // The Bob Pay modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfPaymentApps());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfPaymentApps());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfPaymentApps());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasVisaCapabilities() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        int[] networks = {BasicCardNetwork.VISA};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {
                new ServiceWorkerPaymentApp.Capabilities(networks)};
        installMockServiceWorkerPaymentApp(
                "https://bobpay.com", supportedMethodNames, capabilities, true, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentApps());
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentApps());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentApps());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasMastercardCapabilities() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        int[] networks = {BasicCardNetwork.MASTERCARD};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {
                new ServiceWorkerPaymentApp.Capabilities(networks)};
        installMockServiceWorkerPaymentApp(
                "https://bobpay.com", supportedMethodNames, capabilities, true, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentApps());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentApps());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentApps());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDoNotCallCanMakePayment() throws TimeoutException {
        // Add a credit card to force showing payment sheet UI.
        addCreditCard();
        String[] supportedMethodNames = {"basic-card"};
        installMockServiceWorkerPaymentApp("https://bobpay.com", supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], true, true);

        // Sets setCanMakePaymentForTesting(false) to return false for CanMakePayment since there is
        // no real sw payment app, so if CanMakePayment is called then no payment apps will be
        // available, otherwise CanMakePayment is not called.
        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(false);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfPaymentApps());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanPreselect() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp("https://bobpay.com", supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], true, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        // Payment sheet skips to the app since it is the only available app.
        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getDismissed());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanNotPreselectWithoutName() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp("https://bobpay.com", supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], false, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNull(mPaymentRequestTestRule.getSelectedPaymentAppLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanNotPreselectWithoutIcon() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp("https://bobpay.com", supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], true, false);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNull(mPaymentRequestTestRule.getSelectedPaymentAppLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanNotPreselectWithoutNameAndIcon() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp("https://bobpay.com", supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], false, false);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNull(mPaymentRequestTestRule.getSelectedPaymentAppLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppProvidingShippingComesFirst() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations("https://alicepay.com" /*scope*/,
                false /*shippingAddress*/, false /*payerName*/, false /*payerPhone*/,
                false /*payerEmail*/, "noSupportedDelegation" /*name*/);
        installMockServiceWorkerPaymentAppWithDelegations("https://bobpay.com" /*scope*/,
                true /*shippingAddress*/, false /*payerName*/, false /*payerPhone*/,
                false /*payerEmail*/, "shippingSupported1" /*name */);
        // Install the second app supporting shipping delegation to force showing payment sheet.
        installMockServiceWorkerPaymentAppWithDelegations("https://charliepay.com" /*scope*/,
                true /*shippingAddress*/, false /*payerName*/, false /*payerPhone*/,
                false /*payerEmail*/, "shippingSupported2" /*name */);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_shipping_requested", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(3, mPaymentRequestTestRule.getNumberOfPaymentApps());

        // The payment app which provides shipping address must be preselected.
        Assert.assertTrue(
                mPaymentRequestTestRule.getSelectedPaymentAppLabel().contains("shippingSupported"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppProvidingContactComesFirst() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations("https://alicepay.com" /*scope*/,
                false /*shippingAddress*/, false /*payerName*/, false /*payerPhone*/,
                false /*payerEmail*/, "noSupportedDelegation" /*name*/);
        installMockServiceWorkerPaymentAppWithDelegations("https://bobpay.com" /*scope*/,
                false /*shippingAddress*/, true /*payerName*/, true /*payerPhone*/,
                true /*payerEmail*/, "contactSupported" /*name */);
        installMockServiceWorkerPaymentAppWithDelegations("https://charliepay.com" /*scope*/,
                false /*shippingAddress*/, false /*payerName*/, false /*payerPhone*/,
                true /*payerEmail*/, "emailOnlySupported" /*name */);
        // Install the second app supporting contact delegation to force showing payment sheet.
        installMockServiceWorkerPaymentAppWithDelegations("https://davepay.com" /*scope*/,
                false /*shippingAddress*/, true /*payerName*/, true /*payerPhone*/,
                true /*payerEmail*/, "contactSupported2" /*name */);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_contact_requested", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfPaymentApps());

        // The payment app which provides full contact details must be preselected.
        Assert.assertTrue(
                mPaymentRequestTestRule.getSelectedPaymentAppLabel().contains("contactSupported"));
        // The payment app which partially provides the required contact details comes before the
        // one that provides no contact information.
        Assert.assertTrue(mPaymentRequestTestRule.getPaymentMethodSuggestionLabel(2).contains(
                "emailOnlySupported"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppProvidingAllRequiredInfoComesFirst() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations("https://alicepay.com" /*scope*/,
                true /*shippingAddress*/, false /*payerName*/, false /*payerPhone*/,
                false /*payerEmail*/, "shippingSupported" /*name */);
        installMockServiceWorkerPaymentAppWithDelegations("https://bobpay.com" /*scope*/,
                false /*shippingAddress*/, true /*payerName*/, true /*payerPhone*/,
                true /*payerEmail*/, "contactSupported" /*name */);
        installMockServiceWorkerPaymentAppWithDelegations("https://charliepay.com" /*scope*/,
                true /*shippingAddress*/, true /*payerName*/, true /*payerPhone*/,
                true /*payerEmail*/, "shippingAndContactSupported" /*name*/);
        // Install the second app supporting both shipping and contact delegations to force showing
        // payment sheet.
        installMockServiceWorkerPaymentAppWithDelegations("https://davepay.com" /*scope*/,
                true /*shippingAddress*/, true /*payerName*/, true /*payerPhone*/,
                true /*payerEmail*/, "shippingAndContactSupported2" /*name*/);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait("buy_with_shipping_and_contact_requested",
                mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfPaymentApps());

        // The payment app which provides all required information must be preselected.
        Assert.assertTrue(mPaymentRequestTestRule.getSelectedPaymentAppLabel().contains(
                "shippingAndContactSupported"));
        // The payment app which provides shipping comes before the one which provides contact
        // details when both required by merchant.
        Assert.assertTrue(mPaymentRequestTestRule.getPaymentMethodSuggestionLabel(2).contains(
                "shippingSupported"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSkipsToSinglePaymentAppProvidingShipping() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations("https://alicepay.com" /*scope*/,
                false /*shippingAddress*/, false /*payerName*/, false /*payerPhone*/,
                false /*payerEmail*/, "noSupportedDelegation" /*name*/);
        installMockServiceWorkerPaymentAppWithDelegations("https://bobpay.com" /*scope*/,
                true /*shippingAddress*/, false /*payerName*/, false /*payerPhone*/,
                false /*payerEmail*/, "shippingSupported" /*name */);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "buy_with_shipping_requested", mPaymentRequestTestRule.getDismissed());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSkipsToSinglePaymentAppProvidingContact() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations("https://alicepay.com" /*scope*/,
                false /*shippingAddress*/, false /*payerName*/, false /*payerPhone*/,
                false /*payerEmail*/, "noSupportedDelegation" /*name*/);
        installMockServiceWorkerPaymentAppWithDelegations("https://bobpay.com" /*scope*/,
                false /*shippingAddress*/, true /*payerName*/, true /*payerPhone*/,
                true /*payerEmail*/, "contactSupported" /*name */);
        installMockServiceWorkerPaymentAppWithDelegations("https://charliepay.com" /*scope*/,
                false /*shippingAddress*/, false /*payerName*/, false /*payerPhone*/,
                true /*payerEmail*/, "emailOnlySupported" /*name */);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "buy_with_contact_requested", mPaymentRequestTestRule.getDismissed());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSkipsToSinglePaymentAppProvidingAllRequiredInfo() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations("https://alicepay.com" /*scope*/,
                true /*shippingAddress*/, false /*payerName*/, false /*payerPhone*/,
                false /*payerEmail*/, "shippingSupported" /*name */);
        installMockServiceWorkerPaymentAppWithDelegations("https://bobpay.com" /*scope*/,
                false /*shippingAddress*/, true /*payerName*/, true /*payerPhone*/,
                true /*payerEmail*/, "contactSupported" /*name */);
        installMockServiceWorkerPaymentAppWithDelegations("https://charliepay.com" /*scope*/,
                true /*shippingAddress*/, true /*payerName*/, true /*payerPhone*/,
                true /*payerEmail*/, "shippingAndContactSupported" /*name*/);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "buy_with_shipping_and_contact_requested", mPaymentRequestTestRule.getDismissed());
    }
}
