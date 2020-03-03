// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.os.Bundle;
import android.os.Parcelable;
import android.util.JsonWriter;

import androidx.annotation.Nullable;

import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;

import java.io.IOException;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;

/**
 * The helper that handles intent for AndroidPaymentApp.
 */
public class WebPaymentIntentHelper {
    /** The action name for the Pay Intent. */
    public static final String ACTION_PAY = "org.chromium.intent.action.PAY";

    // Freshest parameters sent to the payment app.
    private static final String EXTRA_CERTIFICATE = "certificate";
    private static final String EXTRA_MERCHANT_NAME = "merchantName";
    private static final String EXTRA_METHOD_DATA = "methodData";
    private static final String EXTRA_METHOD_NAMES = "methodNames";
    private static final String EXTRA_MODIFIERS = "modifiers";
    private static final String EXTRA_PAYMENT_REQUEST_ID = "paymentRequestId";
    private static final String EXTRA_PAYMENT_REQUEST_ORIGIN = "paymentRequestOrigin";
    private static final String EXTRA_TOP_CERTIFICATE_CHAIN = "topLevelCertificateChain";
    private static final String EXTRA_TOP_ORIGIN = "topLevelOrigin";
    private static final String EXTRA_TOTAL = "total";

    // Deprecated parameters sent to the payment app for backward compatibility.
    private static final String EXTRA_DEPRECATED_CERTIFICATE_CHAIN = "certificateChain";
    private static final String EXTRA_DEPRECATED_DATA = "data";
    private static final String EXTRA_DEPRECATED_DATA_MAP = "dataMap";
    private static final String EXTRA_DEPRECATED_DETAILS = "details";
    private static final String EXTRA_DEPRECATED_ID = "id";
    private static final String EXTRA_DEPRECATED_IFRAME_ORIGIN = "iframeOrigin";
    private static final String EXTRA_DEPRECATED_METHOD_NAME = "methodName";
    private static final String EXTRA_DEPRECATED_ORIGIN = "origin";

    private static final String EMPTY_JSON_DATA = "{}";

    /**
     * Build the 'extra' property for an intent.
     *
     * @param id The unique identifier of the PaymentRequest.
     * @param merchantName The name of the merchant.
     * @param schemelessOrigin The schemeless origin of this merchant
     * @param schemelessIframeOrigin The schemeless origin of the iframe that invoked PaymentRequest
     * @param certificateChain The site certificate chain of the merchant. Can be null for localhost
     *                         or local file, which are secure contexts without SSL.
     * @param methodDataMap The payment-method specific data for all applicable payment methods,
     *                         e.g., whether the app should be invoked in test or production, a
     *                         merchant identifier, or a public key.
     * @param total The total amount.
     * @param displayItems The shopping cart items.
     * @param modifiers The relevant payment details modifiers.
     * @return the 'extra' property built for the intent.
     */
    /* package */ static Bundle buildExtras(@Nullable String id, @Nullable String merchantName,
            String schemelessOrigin, String schemelessIframeOrigin,
            @Nullable byte[][] certificateChain, Map<String, PaymentMethodData> methodDataMap,
            @Nullable PaymentItem total, @Nullable List<PaymentItem> displayItems,
            @Nullable Map<String, PaymentDetailsModifier> modifiers) {
        Bundle extras = new Bundle();

        if (id != null) extras.putString(EXTRA_PAYMENT_REQUEST_ID, id);

        if (merchantName != null) extras.putString(EXTRA_MERCHANT_NAME, merchantName);

        extras.putString(EXTRA_TOP_ORIGIN, schemelessOrigin);

        extras.putString(EXTRA_PAYMENT_REQUEST_ORIGIN, schemelessIframeOrigin);

        Parcelable[] serializedCertificateChain = null;
        if (certificateChain != null && certificateChain.length > 0) {
            serializedCertificateChain = buildCertificateChain(certificateChain);
            extras.putParcelableArray(EXTRA_TOP_CERTIFICATE_CHAIN, serializedCertificateChain);
        }

        extras.putStringArrayList(EXTRA_METHOD_NAMES, new ArrayList<>(methodDataMap.keySet()));

        Bundle methodDataBundle = new Bundle();
        for (Map.Entry<String, PaymentMethodData> methodData : methodDataMap.entrySet()) {
            methodDataBundle.putString(methodData.getKey(),
                    methodData.getValue() == null ? EMPTY_JSON_DATA
                                                  : methodData.getValue().stringifiedData);
        }
        extras.putParcelable(EXTRA_METHOD_DATA, methodDataBundle);

        if (modifiers != null) {
            extras.putString(EXTRA_MODIFIERS, serializeModifiers(modifiers.values()));
        }

        if (total != null) {
            String serializedTotalAmount = serializeTotalAmount(total.amount);
            extras.putString(EXTRA_TOTAL,
                    serializedTotalAmount == null ? EMPTY_JSON_DATA : serializedTotalAmount);
        }

        return addDeprecatedExtras(id, schemelessOrigin, schemelessIframeOrigin,
                serializedCertificateChain, methodDataMap, methodDataBundle, total, displayItems,
                extras);
    }

    private static Bundle addDeprecatedExtras(@Nullable String id, String schemelessOrigin,
            String schemelessIframeOrigin, @Nullable Parcelable[] serializedCertificateChain,
            Map<String, PaymentMethodData> methodDataMap, Bundle methodDataBundle,
            @Nullable PaymentItem total, @Nullable List<PaymentItem> displayItems, Bundle extras) {
        if (id != null) extras.putString(EXTRA_DEPRECATED_ID, id);

        extras.putString(EXTRA_DEPRECATED_ORIGIN, schemelessOrigin);

        extras.putString(EXTRA_DEPRECATED_IFRAME_ORIGIN, schemelessIframeOrigin);

        if (serializedCertificateChain != null) {
            extras.putParcelableArray(
                    EXTRA_DEPRECATED_CERTIFICATE_CHAIN, serializedCertificateChain);
        }

        String methodName = methodDataMap.entrySet().iterator().next().getKey();
        extras.putString(EXTRA_DEPRECATED_METHOD_NAME, methodName);

        PaymentMethodData firstMethodData = methodDataMap.get(methodName);
        extras.putString(EXTRA_DEPRECATED_DATA,
                firstMethodData == null ? EMPTY_JSON_DATA : firstMethodData.stringifiedData);

        extras.putParcelable(EXTRA_DEPRECATED_DATA_MAP, methodDataBundle);

        String details = deprecatedSerializeDetails(total, displayItems);
        extras.putString(EXTRA_DEPRECATED_DETAILS, details == null ? EMPTY_JSON_DATA : details);

        return extras;
    }

    private static Parcelable[] buildCertificateChain(byte[][] certificateChain) {
        Parcelable[] result = new Parcelable[certificateChain.length];
        for (int i = 0; i < certificateChain.length; i++) {
            Bundle bundle = new Bundle();
            bundle.putByteArray(EXTRA_CERTIFICATE, certificateChain[i]);
            result[i] = bundle;
        }
        return result;
    }

    private static String deprecatedSerializeDetails(
            @Nullable PaymentItem total, @Nullable List<PaymentItem> displayItems) {
        StringWriter stringWriter = new StringWriter();
        JsonWriter json = new JsonWriter(stringWriter);
        try {
            // details {{{
            json.beginObject();

            if (total != null) {
                // total {{{
                json.name("total");
                serializeTotal(total, json);
                // }}} total
            }

            // displayitems {{{
            if (displayItems != null) {
                json.name("displayItems").beginArray();
                // Do not pass any display items to the payment app.
                json.endArray();
            }
            // }}} displayItems

            json.endObject();
            // }}} details
        } catch (IOException e) {
            return null;
        }

        return stringWriter.toString();
    }

    private static String serializeTotalAmount(PaymentCurrencyAmount totalAmount) {
        StringWriter stringWriter = new StringWriter();
        JsonWriter json = new JsonWriter(stringWriter);
        try {
            // {{{
            json.beginObject();
            json.name("currency").value(totalAmount.currency);
            json.name("value").value(totalAmount.value);
            json.endObject();
            // }}}
        } catch (IOException e) {
            return null;
        }
        return stringWriter.toString();
    }

    private static void serializeTotal(PaymentItem item, JsonWriter json) throws IOException {
        // item {{{
        json.beginObject();
        // Sanitize the total name, because the payment app does not need it to complete the
        // transaction. Matches the behavior of:
        // https://w3c.github.io/payment-handler/#total-attribute
        json.name("label").value("");

        // amount {{{
        json.name("amount").beginObject();
        json.name("currency").value(item.amount.currency);
        json.name("value").value(item.amount.value);
        json.endObject();
        // }}} amount

        json.endObject();
        // }}} item
    }

    private static String serializeModifiers(Collection<PaymentDetailsModifier> modifiers) {
        StringWriter stringWriter = new StringWriter();
        JsonWriter json = new JsonWriter(stringWriter);
        try {
            json.beginArray();
            for (PaymentDetailsModifier modifier : modifiers) {
                serializeModifier(modifier, json);
            }
            json.endArray();
        } catch (IOException e) {
            return EMPTY_JSON_DATA;
        }
        return stringWriter.toString();
    }

    private static void serializeModifier(PaymentDetailsModifier modifier, JsonWriter json)
            throws IOException {
        // {{{
        json.beginObject();

        // total {{{
        if (modifier.total != null) {
            json.name("total");
            serializeTotal(modifier.total, json);
        } else {
            json.name("total").nullValue();
        }
        // }}} total

        // TODO(https://crbug.com/754779): The supportedMethods field was already changed from array
        // to string but we should keep backward-compatibility for now.
        // supportedMethods {{{
        json.name("supportedMethods").beginArray();
        json.value(modifier.methodData.supportedMethod);
        json.endArray();
        // }}} supportedMethods

        // data {{{
        json.name("data").value(modifier.methodData.stringifiedData);
        // }}}

        json.endObject();
        // }}}
    }
}
