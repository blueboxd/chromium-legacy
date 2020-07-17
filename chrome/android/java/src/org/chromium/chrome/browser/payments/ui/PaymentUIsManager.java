// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import org.chromium.chrome.browser.payments.AddressEditor;
import org.chromium.chrome.browser.payments.AutofillPaymentAppCreator;
import org.chromium.chrome.browser.payments.CardEditor;
import org.chromium.chrome.browser.payments.PaymentRequestImpl;

/**
 * This class manages all of the UIs related to payment. The UI logic of {@link PaymentRequestImpl}
 * should be moved into this class.
 */
public class PaymentUIsManager {
    private PaymentRequestUI mPaymentRequestUI;
    private PaymentUisShowStateReconciler mPaymentUisShowStateReconciler;

    private final AddressEditor mAddressEditor;
    private final CardEditor mCardEditor;

    private boolean mMerchantSupportsAutofillCards;
    private SectionInformation mPaymentMethodsSection;
    private SectionInformation mShippingAddressesSection;
    private ContactDetailsSection mContactSection;
    private AutofillPaymentAppCreator mAutofillPaymentAppCreator;

    private boolean mCanUserAddCreditCard;

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
     * @param addressEditor The AddressEditor of the PaymentRequest UI.
     * @param cardEditor The CardEditor of the PaymentRequest UI.
     */
    public PaymentUIsManager(AddressEditor addressEditor, CardEditor cardEditor) {
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
    public boolean merchantSupportsAutofillCards() {
        return mMerchantSupportsAutofillCards;
    }

    /** Set whether the merchant supports autofill cards. */
    public void setMerchantSupportsAutofillCards(boolean merchantSupportsAutofillCards) {
        mMerchantSupportsAutofillCards = merchantSupportsAutofillCards;
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
        return mCanUserAddCreditCard;
    }

    /** Set whether user can add credit card. */
    public void setCanUserAddCreditCard(boolean canUserAddCreditCard) {
        mCanUserAddCreditCard = canUserAddCreditCard;
    }
}
