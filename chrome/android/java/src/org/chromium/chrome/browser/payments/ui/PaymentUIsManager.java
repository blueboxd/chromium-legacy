// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.AddressEditor;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillPaymentAppCreator;
import org.chromium.chrome.browser.payments.AutofillPaymentAppFactory;
import org.chromium.chrome.browser.payments.CardEditor;
import org.chromium.chrome.browser.payments.PaymentRequestImpl;
import org.chromium.chrome.browser.payments.SettingsAutofillAndPaymentsObserver;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppType;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentRequestLifecycleObserver;
import org.chromium.components.payments.PaymentRequestParams;

/**
 * This class manages all of the UIs related to payment. The UI logic of {@link PaymentRequestImpl}
 * should be moved into this class.
 */
public class PaymentUIsManager
        implements SettingsAutofillAndPaymentsObserver.Observer, PaymentRequestLifecycleObserver {
    private final Delegate mDelegate;
    private final AddressEditor mAddressEditor;
    private final CardEditor mCardEditor;
    private final PaymentUisShowStateReconciler mPaymentUisShowStateReconciler;

    private PaymentRequestUI mPaymentRequestUI;

    private Boolean mMerchantSupportsAutofillCards;
    private SectionInformation mPaymentMethodsSection;
    private SectionInformation mShippingAddressesSection;
    private ContactDetailsSection mContactSection;
    private AutofillPaymentAppCreator mAutofillPaymentAppCreator;

    private Boolean mCanUserAddCreditCard;

    /** The delegate of this class. */
    public interface Delegate {
        /** Updates the modifiers for payment apps and order summary. */
        void updateAppModifiedTotals();
    }

    /**
     * This class is to coordinate the show state of a bottom sheet UI (either expandable payment
     * handler or minimal UI) and the Payment Request UI so that these visibility rules are
     * enforced:
     * 1. At most one UI is shown at any moment in case the Payment Request UI obstructs the bottom
     * sheet.
     * 2. Bottom sheet is prioritized to show over Payment Request UI
     */
    public class PaymentUisShowStateReconciler {
        // Whether the bottom sheet is showing.
        private boolean mShowingBottomSheet;
        // Whether to show the Payment Request UI when the bottom sheet is not being shown.
        private boolean mShouldShowDialog;

        /**
         * Show the Payment Request UI dialog when the bottom sheet is hidden, i.e., if the bottom
         * sheet hidden, show the dialog immediately; otherwise, show the dialog after the bottom
         * sheet hides.
         */
        /* package */ void showPaymentRequestDialogWhenNoBottomSheet() {
            mShouldShowDialog = true;
            updatePaymentRequestDialogShowState();
        }

        /** Hide the Payment Request UI dialog. */
        /* package */ void hidePaymentRequestDialog() {
            mShouldShowDialog = false;
            updatePaymentRequestDialogShowState();
        }

        /** A callback invoked when the Payment Request UI is closed. */
        public void onPaymentRequestUiClosed() {
            assert mPaymentRequestUI == null;
            mShouldShowDialog = false;
        }

        /** A callback invoked when the bottom sheet is shown, to enforce the visibility rules. */
        public void onBottomSheetShown() {
            mShowingBottomSheet = true;
            updatePaymentRequestDialogShowState();
        }

        /** A callback invoked when the bottom sheet is hidden, to enforce the visibility rules. */
        public void onBottomSheetClosed() {
            mShowingBottomSheet = false;
            updatePaymentRequestDialogShowState();
        }

        private void updatePaymentRequestDialogShowState() {
            if (mPaymentRequestUI == null) return;
            mPaymentRequestUI.setVisible(!mShowingBottomSheet && mShouldShowDialog);
        }
    }

    /**
     * Create PaymentUIsManager.
     * @param delegate The delegate of this class.
     * @param addressEditor The AddressEditor of the PaymentRequest UI.
     * @param cardEditor The CardEditor of the PaymentRequest UI.
     */
    // TODO(crbug.com/1107102): AddressEditor and CardEditor should be initialized in this
    // constructor instead of the caller of the constructor, once CardEditor's "ForTest" symbols
    // have been removed from the production code.
    public PaymentUIsManager(
            Delegate delegate, AddressEditor addressEditor, CardEditor cardEditor) {
        mDelegate = delegate;
        mAddressEditor = addressEditor;
        mCardEditor = cardEditor;
        mPaymentUisShowStateReconciler = new PaymentUisShowStateReconciler();
    }

    /** @return The PaymentRequestUI. */
    public PaymentRequestUI getPaymentRequestUI() {
        return mPaymentRequestUI;
    }

    /**
     * Set the PaymentRequestUI.
     * @param paymentRequestUI The PaymentRequestUI.
     */
    public void setPaymentRequestUI(PaymentRequestUI paymentRequestUI) {
        mPaymentRequestUI = paymentRequestUI;
    }

    /** @return The PaymentUisShowStateReconciler. */
    public PaymentUisShowStateReconciler getPaymentUisShowStateReconciler() {
        return mPaymentUisShowStateReconciler;
    }

    /** @return Get the AddressEditor of the PaymentRequest UI. */
    public AddressEditor getAddressEditor() {
        return mAddressEditor;
    }

    /** @return Get the CardEditor of the PaymentRequest UI. */
    public CardEditor getCardEditor() {
        return mCardEditor;
    }

    /** @return Whether the merchant supports autofill cards. */
    @Nullable
    public Boolean merchantSupportsAutofillCards() {
        // TODO(crbug.com/1107039): this value should be asserted not null to avoid being used
        // before defined, after this bug is fixed.
        return mMerchantSupportsAutofillCards;
    }

    /** @return Get the PaymentMethodsSection of the PaymentRequest UI. */
    public SectionInformation getPaymentMethodsSection() {
        return mPaymentMethodsSection;
    }

    /** Set the PaymentMethodsSection of the PaymentRequest UI. */
    public void setPaymentMethodsSection(SectionInformation paymentMethodsSection) {
        mPaymentMethodsSection = paymentMethodsSection;
    }

    /** Get the ShippingAddressesSection of the PaymentRequest UI. */
    public SectionInformation getShippingAddressesSection() {
        return mShippingAddressesSection;
    }

    /** Set the ShippingAddressesSection of the PaymentRequest UI. */
    public void setShippingAddressesSection(SectionInformation shippingAddressesSection) {
        mShippingAddressesSection = shippingAddressesSection;
    }

    /** Get the ContactSection of the PaymentRequest UI. */
    public ContactDetailsSection getContactSection() {
        return mContactSection;
    }

    /** Set the ContactSection of the PaymentRequest UI. */
    public void setContactSection(ContactDetailsSection contactSection) {
        mContactSection = contactSection;
    }

    /** Get the AutofillPaymentAppCreator. */
    public AutofillPaymentAppCreator getAutofillPaymentAppCreator() {
        return mAutofillPaymentAppCreator;
    }

    /** Set the AutofillPaymentAppCreator. */
    public void setAutofillPaymentAppCreator(AutofillPaymentAppCreator autofillPaymentAppCreator) {
        mAutofillPaymentAppCreator = autofillPaymentAppCreator;
    }

    /** @return Whether user can add credit card. */
    public boolean canUserAddCreditCard() {
        assert mCanUserAddCreditCard != null;
        return mCanUserAddCreditCard;
    }

    // Implement SettingsAutofillAndPaymentsObserver.Observer:
    @Override
    public void onAddressUpdated(AutofillAddress address) {
        address.setShippingAddressLabelWithCountry();
        mCardEditor.updateBillingAddressIfComplete(address);

        if (mShippingAddressesSection != null) {
            mShippingAddressesSection.addAndSelectOrUpdateItem(address);
            mPaymentRequestUI.updateSection(
                    PaymentRequestUI.DataType.SHIPPING_ADDRESSES, mShippingAddressesSection);
        }

        if (mContactSection != null) {
            mContactSection.addOrUpdateWithAutofillAddress(address);
            mPaymentRequestUI.updateSection(
                    PaymentRequestUI.DataType.CONTACT_DETAILS, mContactSection);
        }
    }

    // Implement SettingsAutofillAndPaymentsObserver.Observer:
    @Override
    public void onAddressDeleted(String guid) {
        // TODO: Delete the address from getShippingAddressesSection() and
        // getContactSection(). Note that we only displayed
        // SUGGESTIONS_LIMIT addresses, so we may want to add back previously ignored addresses.
    }

    // Implement SettingsAutofillAndPaymentsObserver.Observer:
    @Override
    public void onCreditCardUpdated(CreditCard card) {
        assert mMerchantSupportsAutofillCards != null;
        if (!mMerchantSupportsAutofillCards || mPaymentMethodsSection == null
                || mAutofillPaymentAppCreator == null) {
            return;
        }

        PaymentApp updatedAutofillCard = mAutofillPaymentAppCreator.createPaymentAppForCard(card);

        // Can be null when the card added through settings does not match the requested card
        // network or is invalid, because autofill settings do not perform the same level of
        // validation as Basic Card implementation in Chrome.
        if (updatedAutofillCard == null) return;

        mPaymentMethodsSection.addAndSelectOrUpdateItem(updatedAutofillCard);

        mDelegate.updateAppModifiedTotals();

        if (mPaymentRequestUI != null) {
            mPaymentRequestUI.updateSection(
                    PaymentRequestUI.DataType.PAYMENT_METHODS, mPaymentMethodsSection);
        }
    }

    // Implement SettingsAutofillAndPaymentsObserver.Observer:
    @Override
    public void onCreditCardDeleted(String guid) {
        assert mMerchantSupportsAutofillCards != null;
        if (!mMerchantSupportsAutofillCards || mPaymentMethodsSection == null) return;

        mPaymentMethodsSection.removeAndUnselectItem(guid);

        mDelegate.updateAppModifiedTotals();

        if (mPaymentRequestUI != null) {
            mPaymentRequestUI.updateSection(
                    PaymentRequestUI.DataType.PAYMENT_METHODS, mPaymentMethodsSection);
        }
    }

    // Implement PaymentRequestLifecycleObserver:
    @Override
    public void onPaymentRequestParamsInitiated(PaymentRequestParams params) {
        // Checks whether the merchant supports autofill cards before show is called.
        mMerchantSupportsAutofillCards =
                AutofillPaymentAppFactory.merchantSupportsBasicCard(params.getMethodDataMap());

        // If in strict mode, don't give user an option to add an autofill card during the checkout
        // to avoid the "unhappy" basic-card flow.
        mCanUserAddCreditCard = mMerchantSupportsAutofillCards
                && !PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                        PaymentFeatureList.STRICT_HAS_ENROLLED_AUTOFILL_INSTRUMENT);
    }

    /** @return The selected payment app type. */
    public @PaymentAppType int getSelectedPaymentAppType() {
        return mPaymentMethodsSection != null && mPaymentMethodsSection.getSelectedItem() != null
                ? ((PaymentApp) mPaymentMethodsSection.getSelectedItem()).getPaymentAppType()
                : PaymentAppType.UNDEFINED;
    }
}
