// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static com.google.common.truth.Truth.assertThat;

import android.content.Intent;
import android.os.Bundle;
import android.os.Parcelable;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.intent.WebPaymentIntentHelper;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentCurrencyAmount;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentDetailsModifier;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentItem;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentMethodData;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Tests for {@link WebPaymentIntentHelper}.
 **/
@RunWith(ChromeJUnit4ClassRunner.class)
public class WebPaymentIntentHelperTest {
    @Rule
    public ExpectedException thrown = ExpectedException.none();

    // Test the happy path of createPayIntent and verify the non-deprecated extras.
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void createPayIntentTest() throws Throwable {
        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "{\"key\":\"value\"}");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        List<PaymentItem> displayItems = new ArrayList<PaymentItem>();
        displayItems.add(new PaymentItem(new PaymentCurrencyAmount("CAD", "50")));
        displayItems.add(new PaymentItem(new PaymentCurrencyAmount("CAD", "150")));

        Map<String, PaymentDetailsModifier> modifiers =
                new HashMap<String, PaymentDetailsModifier>();
        PaymentDetailsModifier bobPaymodifier = new PaymentDetailsModifier(total, bobPayMethodData);
        modifiers.put("bobPay", bobPaymodifier);

        byte[][] certificateChain = new byte[][] {{0}};

        Intent intent = WebPaymentIntentHelper.createPayIntent("package.name", "activity.name",
                "payment.request.id", "merchant.name", "schemeless.origin",
                "schemeless.iframe.origin", certificateChain, methodDataMap, total, displayItems,
                modifiers);
        Bundle bundle = intent.getExtras();
        Assert.assertNotNull(bundle);
        Assert.assertEquals(
                "payment.request.id", bundle.get(WebPaymentIntentHelper.EXTRA_PAYMENT_REQUEST_ID));
        Assert.assertEquals(
                "merchant.name", bundle.get(WebPaymentIntentHelper.EXTRA_MERCHANT_NAME));
        Assert.assertEquals(
                "schemeless.origin", bundle.get(WebPaymentIntentHelper.EXTRA_TOP_ORIGIN));
        Assert.assertEquals("schemeless.iframe.origin",
                bundle.get(WebPaymentIntentHelper.EXTRA_PAYMENT_REQUEST_ORIGIN));

        Parcelable[] certificateChainParcels =
                bundle.getParcelableArray(WebPaymentIntentHelper.EXTRA_TOP_CERTIFICATE_CHAIN);
        Assert.assertEquals(1, certificateChainParcels.length);
        assertThat(((Bundle) certificateChainParcels[0])
                           .getByteArray(WebPaymentIntentHelper.EXTRA_CERTIFICATE))
                .isEqualTo(new byte[] {0});

        Assert.assertEquals(Arrays.asList("bobPay"),
                bundle.getStringArrayList(WebPaymentIntentHelper.EXTRA_METHOD_NAMES));

        Bundle expectedMethodDataBundle =
                bundle.getParcelable(WebPaymentIntentHelper.EXTRA_METHOD_DATA);
        Assert.assertEquals(1, expectedMethodDataBundle.keySet().size());
        Assert.assertEquals("{\"key\":\"value\"}", expectedMethodDataBundle.getString("bobPay"));

        // The data field is a string because it is PaymentMethodData#stringifiedData.
        String expectedSerializedModifiers =
                "[{\"total\":{\"label\":\"\",\"amount\":{\"currency\":\"CAD\",\"value\":\"200\"}},"
                + "\"supportedMethods\":[\"method\"],"
                + "\"data\":\"{\\\"key\\\":\\\"value\\\"}\"}]";
        Assert.assertEquals(
                expectedSerializedModifiers, bundle.get(WebPaymentIntentHelper.EXTRA_MODIFIERS));
        Assert.assertEquals("{\"currency\":\"CAD\",\"value\":\"200\"}",
                bundle.get(WebPaymentIntentHelper.EXTRA_TOTAL));
    }

    // Test the happy path of createPayIntent and verify the deprecated extras.
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void createPayIntentDeprecatedExtraTest() throws Throwable {
        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        List<PaymentItem> displayItems = new ArrayList<PaymentItem>();
        displayItems.add(new PaymentItem(new PaymentCurrencyAmount("CAD", "50")));
        displayItems.add(new PaymentItem(new PaymentCurrencyAmount("CAD", "150")));

        Map<String, PaymentDetailsModifier> modifiers =
                new HashMap<String, PaymentDetailsModifier>();
        PaymentDetailsModifier modifier = new PaymentDetailsModifier(total, bobPayMethodData);
        modifiers.put("modifier_key", modifier);

        byte[][] certificateChain = new byte[][] {{0}};

        Intent intent = WebPaymentIntentHelper.createPayIntent("package.name", "activity.name",
                "payment.request.id", "merchant.name", "schemeless.origin",
                "schemeless.iframe.origin", certificateChain, methodDataMap, total, displayItems,
                modifiers);
        Bundle bundle = intent.getExtras();
        Assert.assertNotNull(bundle);

        Assert.assertEquals(
                "payment.request.id", bundle.get(WebPaymentIntentHelper.EXTRA_DEPRECATED_ID));
        Assert.assertEquals(
                "schemeless.origin", bundle.get(WebPaymentIntentHelper.EXTRA_DEPRECATED_ORIGIN));
        Assert.assertEquals("schemeless.iframe.origin",
                bundle.get(WebPaymentIntentHelper.EXTRA_DEPRECATED_IFRAME_ORIGIN));

        Parcelable[] deprecatedCertificateChainParcels = bundle.getParcelableArray(
                WebPaymentIntentHelper.EXTRA_DEPRECATED_CERTIFICATE_CHAIN);
        Assert.assertEquals(1, deprecatedCertificateChainParcels.length);
        assertThat(((Bundle) deprecatedCertificateChainParcels[0])
                           .getByteArray(WebPaymentIntentHelper.EXTRA_CERTIFICATE))
                .isEqualTo(new byte[] {0});

        Assert.assertEquals(
                "bobPay", bundle.get(WebPaymentIntentHelper.EXTRA_DEPRECATED_METHOD_NAME));
        Assert.assertEquals("null", bundle.get(WebPaymentIntentHelper.EXTRA_DEPRECATED_DATA));

        Bundle deprecatedDataMap =
                bundle.getParcelable(WebPaymentIntentHelper.EXTRA_DEPRECATED_DATA_MAP);
        Assert.assertEquals(1, deprecatedDataMap.keySet().size());
        Assert.assertEquals("null", deprecatedDataMap.getString("bobPay"));

        Assert.assertEquals("{\"total\":{\"label\":\"\","
                        + "\"amount\":{\"currency\":\"CAD\",\"value\":\"200\"}},"
                        + "\"displayItems\":[]}",
                bundle.get(WebPaymentIntentHelper.EXTRA_DEPRECATED_DETAILS));
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void nullPackageNameExceptionTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("packageName should not be null or empty.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent(/*packageName=*/null, "activity.name",
                "payment.request.id", "merchant.name", "schemeless.origin",
                "schemeless.iframe.origin", /*certificateChain=*/null, methodDataMap, total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void nullActivityNameExceptionTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("activityName should not be null or empty.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", /*activityName=*/null,
                "payment.request.id", "merchant.name", "schemeless.origin",
                "schemeless.iframe.origin", /*certificateChain=*/null, methodDataMap, total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void nullIdExceptionTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("id should not be null or empty.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name",
                /*id=*/null, "merchant.name", "schemeless.origin", "schemeless.iframe.origin",
                /*certificateChain=*/null, methodDataMap, total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void emptyIdExceptionTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("id should not be null or empty.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name",
                /*id=*/"", "merchant.name", "schemeless.origin", "schemeless.iframe.origin",
                /*certificateChain=*/null, methodDataMap, total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void nullMerchantNameExceptionTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("merchantName should not be null.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name", "id",
                /*merchantName=*/null, "schemeless.origin", "schemeless.iframe.origin",
                /*certificateChain=*/null, methodDataMap, total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void emptyMerchantNameNoExceptionTest() throws Throwable {
        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        Intent payIntent =
                WebPaymentIntentHelper.createPayIntent("package.name", "activity.name", "id",
                        /*merchantName=*/"", "schemeless.origin", "schemeless.iframe.origin",
                        /*certificateChain=*/null, methodDataMap, total,
                        /*displayItems=*/null,
                        /*modifiers=*/null);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void nullOriginExceptionTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("schemelessOrigin should not be null or empty.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name", "id",
                "merchant.name", /*schemelessOrigin=*/null, "schemeless.iframe.origin",
                /*certificateChain=*/null, methodDataMap, total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void emptyOriginExceptionTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("schemelessOrigin should not be null or empty.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name", "id",
                "merchant.name", /*schemelessOrigin=*/"", "schemeless.iframe.origin",
                /*certificateChain=*/null, methodDataMap, total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void nullIframeOriginExceptionTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("schemelessIframeOrigin should not be null or empty.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name", "id",
                "merchant.name", "schemeless.origin",
                /*schemelessIframeOrigin=*/null, /*certificateChain=*/null, methodDataMap, total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void emptyIframeOriginExceptionTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("schemelessIframeOrigin should not be null or empty.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name", "id",
                "merchant.name", "schemeless.origin",
                /*schemelessIframeOrigin=*/"", /*certificateChain=*/null, methodDataMap, total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void nullByteArrayCertifiateChainExceptionTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("certificateChain[0] should not be null.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        byte[][] certificateChain = new byte[][] {null};

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name", "id",
                "merchant.name", "schemeless.origin", "schemeless.iframe.origin", certificateChain,
                methodDataMap, total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    // Verify that a null value in methodDataMap would trigger an exception.
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void methodDataMapNullValueTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("methodDataMap's entry value should not be null.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        methodDataMap.put("bobPay", null);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name",
                "payment.request.id", "merchant.name", "schemeless.origin",
                "schemeless.iframe.origin", /*certificateChain=*/null, methodDataMap, total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    // Verify that a null methodDataMap would trigger an exception.
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void nullMethodDataMapTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("methodDataMap should not be null or empty.");

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name",
                "payment.request.id", "merchant.name", "schemeless.origin",
                "schemeless.iframe.origin", /*certificateChain=*/null, /*methodDataMap=*/null,
                total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    // Verify that an empty methodDataMap would trigger an exception.
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void emptyMethodDataMapTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("methodDataMap should not be null or empty.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name",
                "payment.request.id", "merchant.name", "schemeless.origin",
                "schemeless.iframe.origin", /*certificateChain=*/null, methodDataMap, total,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void nullTotalTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("total should not be null.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name", "id",
                "merchant.name", "schemeless.origin", "schemeless.iframe.origin",
                /*certificateChain=*/null, methodDataMap,
                /*total=*/null,
                /*displayItems=*/null,
                /*modifiers=*/null);
    }

    // Verify that a null value in the modifier map would trigger an exception.
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void modifierMapNullValueTest() throws Throwable {
        thrown.expect(IllegalArgumentException.class);
        thrown.expectMessage("PaymentDetailsModifier should not be null.");

        Map<String, PaymentMethodData> methodDataMap = new HashMap<String, PaymentMethodData>();
        PaymentMethodData bobPayMethodData = new PaymentMethodData("method", "null");
        methodDataMap.put("bobPay", bobPayMethodData);

        PaymentItem total = new PaymentItem(new PaymentCurrencyAmount("CAD", "200"));

        Map<String, PaymentDetailsModifier> modifiers =
                new HashMap<String, PaymentDetailsModifier>();
        modifiers.put("bobPay", null);

        WebPaymentIntentHelper.createPayIntent("package.name", "activity.name",
                "payment.request.id", "merchant.name", "schemeless.origin",
                "schemeless.iframe.origin", /*certificateChain=*/null, methodDataMap, total,
                /*displayItems=*/null, modifiers);
    }
}
