// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/** Place to define and control payment preferences. */
public class PaymentPreferencesUtil {
    // Avoid instantiation by accident.
    private PaymentPreferencesUtil() {}

    /**
     * Checks whehter the payment request has been successfully completed once.
     *
     * @return True If payment request has been successfully completed once.
     */
    public static boolean isPaymentCompleteOnce() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.PAYMENTS_PAYMENT_COMPLETE_ONCE, false);
    }

    /** Sets the payment request has been successfully completed once. */
    public static void setPaymentCompleteOnce() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.PAYMENTS_PAYMENT_COMPLETE_ONCE, true);
    }

    /**
     * Gets use count of the payment instrument.
     *
     * @param id The instrument identifier.
     * @return The use count.
     */
    public static int getPaymentInstrumentUseCount(String id) {
        return SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.PAYMENTS_PAYMENT_INSTRUMENT_USE_COUNT.createKey(id));
    }

    /**
     * Increase use count of the payment instrument by one.
     *
     * @param id The instrument identifier.
     */
    public static void increasePaymentInstrumentUseCount(String id) {
        SharedPreferencesManager.getInstance().incrementInt(
                ChromePreferenceKeys.PAYMENTS_PAYMENT_INSTRUMENT_USE_COUNT.createKey(id));
    }

    /**
     * A convenient method to set use count of the payment instrument to a specific value for test.
     *
     * @param id    The instrument identifier.
     * @param count The count value.
     */
    @VisibleForTesting
    public static void setPaymentInstrumentUseCountForTest(String id, int count) {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.PAYMENTS_PAYMENT_INSTRUMENT_USE_COUNT.createKey(id), count);
    }

    /**
     * Gets last use date of the payment instrument.
     *
     * @param id The instrument identifier.
     * @return The time difference between the last use date and 'midnight, January 1, 1970 UTC' in
     *         milliseconds.
     */
    public static long getPaymentInstrumentLastUseDate(String id) {
        return SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.PAYMENTS_PAYMENT_INSTRUMENT_USE_DATE.createKey(id));
    }

    /**
     * Sets last use date of the payment instrument.
     *
     * @param id   The instrument identifier.
     * @param date The time difference between the last use date and 'midnight, January 1, 1970 UTC'
     *             in milliseconds.
     */
    public static void setPaymentInstrumentLastUseDate(String id, long date) {
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.PAYMENTS_PAYMENT_INSTRUMENT_USE_DATE.createKey(id), date);
    }
}
