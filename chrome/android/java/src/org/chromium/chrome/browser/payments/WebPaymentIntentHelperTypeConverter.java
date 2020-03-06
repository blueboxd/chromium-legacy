// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.Nullable;

import org.chromium.base.CollectionUtil;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This class defines the utility functions that convert the payment info types in
 * org.chromium.payments.mojom to their counterparts in WebPaymentIntentHelperType.
 */
public final class WebPaymentIntentHelperTypeConverter {
    public static WebPaymentIntentHelperType.PaymentCurrencyAmount fromMojoPaymentCurrencyAmount(
            @Nullable PaymentCurrencyAmount currencyAmount) {
        if (currencyAmount == null) return null;
        return new WebPaymentIntentHelperType.PaymentCurrencyAmount(
                /*currency=*/currencyAmount.currency, /*value=*/currencyAmount.value);
    }

    public static WebPaymentIntentHelperType.PaymentItem fromMojoPaymentItem(
            @Nullable PaymentItem item) {
        if (item == null) return null;
        return new WebPaymentIntentHelperType.PaymentItem(
                fromMojoPaymentCurrencyAmount(item.amount));
    }

    public static WebPaymentIntentHelperType.PaymentDetailsModifier fromMojoPaymentDetailsModifier(
            @Nullable PaymentDetailsModifier detailsModifier) {
        if (detailsModifier == null) return null;
        return new WebPaymentIntentHelperType.PaymentDetailsModifier(
                fromMojoPaymentItem(detailsModifier.total),
                fromMojoPaymentMethodData(detailsModifier.methodData));
    }

    public static WebPaymentIntentHelperType.PaymentMethodData fromMojoPaymentMethodData(
            @Nullable PaymentMethodData methodData) {
        if (methodData == null) return null;
        return new WebPaymentIntentHelperType.PaymentMethodData(
                /*supportedMethod=*/methodData.supportedMethod,
                /*stringifiedData=*/methodData.stringifiedData);
    }

    public static Map<String, WebPaymentIntentHelperType.PaymentMethodData>
    fromMojoPaymentMethodDataMap(@Nullable Map<String, PaymentMethodData> methodDataMap) {
        if (methodDataMap == null) return null;
        Map<String, WebPaymentIntentHelperType.PaymentMethodData> compatibleMethodDataMap =
                new HashMap<>();
        CollectionUtil.forEach(methodDataMap,
                entry
                -> compatibleMethodDataMap.put(entry.getKey(),
                        WebPaymentIntentHelperTypeConverter.fromMojoPaymentMethodData(
                                entry.getValue())));
        return compatibleMethodDataMap;
    }

    public static Map<String, WebPaymentIntentHelperType.PaymentDetailsModifier>
    fromMojoPaymentDetailsModifierMap(@Nullable Map<String, PaymentDetailsModifier> modifiers) {
        if (modifiers == null) return null;
        Map<String, WebPaymentIntentHelperType.PaymentDetailsModifier> compatibleModifiers =
                new HashMap<>();
        CollectionUtil.forEach(modifiers,
                entry
                -> compatibleModifiers.put(entry.getKey(),
                        WebPaymentIntentHelperTypeConverter.fromMojoPaymentDetailsModifier(
                                entry.getValue())));
        return compatibleModifiers;
    }

    public static List<WebPaymentIntentHelperType.PaymentItem> fromMojoPaymentItems(
            @Nullable List<PaymentItem> paymentItems) {
        if (paymentItems == null) return null;
        List<WebPaymentIntentHelperType.PaymentItem> compatiblePaymentItems = new ArrayList<>();
        CollectionUtil.forEach(paymentItems,
                element
                -> compatiblePaymentItems.add(
                        WebPaymentIntentHelperTypeConverter.fromMojoPaymentItem(element)));
        return compatiblePaymentItems;
    }
}
