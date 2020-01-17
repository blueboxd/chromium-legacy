// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.payments.MethodStrings;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.List;
import java.util.Map;
import java.util.Set;

/** Creates one payment app per card on file in Autofill. */
public class AutofillPaymentAppFactory implements PaymentAppFactoryInterface {
    private final Handler mHandler = new Handler();

    // PaymentAppFactoryInterface implementation.
    @Override
    public void create(PaymentAppFactoryDelegate delegate) {
        Creator creator = new Creator(delegate);
        mHandler.post(() -> {
            boolean canMakePayment = creator.createPaymentApps();
            delegate.onCanMakePaymentCalculated(canMakePayment);
            delegate.onDoneCreatingPaymentApps(AutofillPaymentAppFactory.this);
        });
    }

    /** Creates one payment app per Autofill card on file that matches the given payment request. */
    private static final class Creator implements AutofillPaymentAppCreator {
        private final PaymentAppFactoryDelegate mDelegate;
        private boolean mCanMakePayment;
        private Set<Integer> mTypes;
        private Set<String> mNetworks;

        private Creator(PaymentAppFactoryDelegate delegate) {
            mDelegate = delegate;
        }

        /** @return Whether can make payments with basic card. */
        private boolean createPaymentApps() {
            if (!mDelegate.getParams().getMethodData().containsKey(MethodStrings.BASIC_CARD)) {
                return false;
            }

            PaymentMethodData data =
                    mDelegate.getParams().getMethodData().get(MethodStrings.BASIC_CARD);
            mNetworks = BasicCardUtils.convertBasicCardToNetworks(data);
            if (mNetworks.isEmpty()) return false;

            mTypes = BasicCardUtils.convertBasicCardToTypes(data);
            if (mTypes.isEmpty()) return false;

            mCanMakePayment = true;
            List<CreditCard> cards = PersonalDataManager.getInstance().getCreditCardsToSuggest(
                    /*includeServerCards=*/ChromeFeatureList.isEnabled(
                            ChromeFeatureList.WEB_PAYMENTS_RETURN_GOOGLE_PAY_IN_BASIC_CARD));
            int numberOfCards = cards.size();
            for (int i = 0; i < numberOfCards; i++) {
                // createPaymentAppForCard(card) returns null if the card network or type does not
                // match mNetworks or mTypes.
                PaymentInstrument app = createPaymentAppForCard(cards.get(i));
                if (app != null) mDelegate.onPaymentAppCreated(app);
            }

            int additionalAppTextResourceId = getAdditionalAppTextResourceId();
            if (additionalAppTextResourceId != 0) {
                mDelegate.onAdditionalTextResourceId(additionalAppTextResourceId);
            }

            mDelegate.onAutofillPaymentAppCreatorAvailable(this);
            return true;
        }

        // AutofillPaymentAppCreator interface.
        @Override
        @Nullable
        public PaymentInstrument createPaymentAppForCard(CreditCard card) {
            if (!mCanMakePayment) return null;

            String methodName = null;
            if (!mNetworks.contains(card.getBasicCardIssuerNetwork())
                    || !mTypes.contains(card.getCardType())) {
                return null;
            }

            AutofillProfile billingAddress = TextUtils.isEmpty(card.getBillingAddressId())
                    ? null
                    : PersonalDataManager.getInstance().getProfile(card.getBillingAddressId());

            if (billingAddress != null
                    && AutofillAddress.checkAddressCompletionStatus(
                               billingAddress, AutofillAddress.CompletenessCheckType.IGNORE_PHONE)
                            != AutofillAddress.CompletionStatus.COMPLETE) {
                billingAddress = null;
            }

            if (billingAddress == null) card.setBillingAddressId(null);

            // Whether this card matches the card type (credit, debit, prepaid) exactly. If the
            // merchant requests all card types, then this is always true. If the merchant requests
            // only a subset of card types, then this is false for "unknown" card types. The
            // "unknown" card types is where Chrome is unable to determine the type of card. Cards
            // that don't match the card type exactly cannot be pre-selected in the UI.
            boolean matchesMerchantCardTypeExactly = card.getCardType() != CardType.UNKNOWN
                    || mTypes.size() == BasicCardUtils.TOTAL_NUMBER_OF_CARD_TYPES;

            return new AutofillPaymentInstrument(mDelegate.getParams().getWebContents(), card,
                    billingAddress, MethodStrings.BASIC_CARD, matchesMerchantCardTypeExactly);
        }

        private int getAdditionalAppTextResourceId() {
            // If the merchant has restricted the accepted card types (credit, debit, prepaid), then
            // the list of payment instruments should include a message describing the accepted card
            // types, e.g., "Debit cards are accepted" or "Debit and prepaid cards are accepted."
            if (mTypes == null || mTypes.size() == BasicCardUtils.TOTAL_NUMBER_OF_CARD_TYPES) {
                return 0;
            }

            int credit = mTypes.contains(CardType.CREDIT) ? 1 : 0;
            int debit = mTypes.contains(CardType.DEBIT) ? 1 : 0;
            int prepaid = mTypes.contains(CardType.PREPAID) ? 1 : 0;
            int[][][] resourceIds = new int[2][2][2];
            resourceIds[0][0][0] = 0;
            resourceIds[0][0][1] = R.string.payments_prepaid_cards_are_accepted_label;
            resourceIds[0][1][0] = R.string.payments_debit_cards_are_accepted_label;
            resourceIds[0][1][1] = R.string.payments_debit_prepaid_cards_are_accepted_label;
            resourceIds[1][0][0] = R.string.payments_credit_cards_are_accepted_label;
            resourceIds[1][0][1] = R.string.payments_credit_prepaid_cards_are_accepted_label;
            resourceIds[1][1][0] = R.string.payments_credit_debit_cards_are_accepted_label;
            resourceIds[1][1][1] = 0;
            return resourceIds[credit][debit][prepaid];
        }
    }

    /** @return True if the merchant methodDataMap supports autofill payment instruments. */
    public static boolean merchantSupportsAutofillPaymentInstruments(
            Map<String, PaymentMethodData> methodDataMap) {
        assert methodDataMap != null;
        PaymentMethodData basicCardData = methodDataMap.get(MethodStrings.BASIC_CARD);
        if (basicCardData != null) {
            Set<String> basicCardNetworks =
                    BasicCardUtils.convertBasicCardToNetworks(basicCardData);
            if (basicCardNetworks != null && !basicCardNetworks.isEmpty()) return true;
        }

        // Card issuer networks as payment method names was removed in Chrome 77.
        // https://www.chromestatus.com/feature/5725727580225536
        return false;
    }

    /**
     * Checks for usable Autofill card on file.
     *
     * @param webContents The web contents where PaymentRequest was invoked.
     * @param methodData The payment methods and their corresponding data.
     * @return Whether there's a usable Autofill card on file.
     */
    public static boolean hasUsableAutofillCard(
            WebContents webContents, Map<String, PaymentMethodData> methodData) {
        PaymentAppFactoryParams params = new PaymentAppFactoryParams() {
            @Override
            public WebContents getWebContents() {
                return webContents;
            }

            @Override
            public Map<String, PaymentMethodData> getMethodData() {
                return methodData;
            }
        };
        final class UsableCardFinder implements PaymentAppFactoryDelegate {
            private boolean mResult;

            @Override
            public PaymentAppFactoryParams getParams() {
                return params;
            }

            @Override
            public void onPaymentAppCreated(PaymentInstrument app) {
                app.setHaveRequestedAutofillData(true);
                assert app instanceof AutofillPaymentInstrument;
                if (((AutofillPaymentInstrument) app).strictCanMakePayment()) mResult = true;
            }
        };
        UsableCardFinder usableCardFinder = new UsableCardFinder();
        Creator creator = new Creator(/*delegate=*/usableCardFinder);
        // Synchronously invokes usableCardFinder.onPaymentAppCreated(app).
        creator.createPaymentApps();
        return usableCardFinder.mResult;
    }
}
