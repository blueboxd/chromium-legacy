// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

// TODO(crbug.com/806868): Use mCarouselCoordinator to show chips.

/**
 * Coordinator for the Payment Request.
 */
public class AssistantPaymentRequestCoordinator {
    public static final String DIVIDER_TAG = "divider";
    private final Activity mActivity;
    private final LinearLayout mPaymentRequestUI;
    private final AssistantPaymentRequestModel mModel;
    private AssistantPaymentRequestBinder.ViewHolder mViewHolder;

    public AssistantPaymentRequestCoordinator(
            Activity activity, AssistantPaymentRequestModel model) {
        mActivity = activity;
        mModel = model;
        int sectionToSectionPadding = activity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_payment_request_section_padding);

        mPaymentRequestUI = new LinearLayout(mActivity);
        mPaymentRequestUI.setOrientation(LinearLayout.VERTICAL);
        mPaymentRequestUI.setLayoutParams(
                new ViewGroup.LayoutParams(/* width= */ ViewGroup.LayoutParams.MATCH_PARENT,
                        /* height= */ ViewGroup.LayoutParams.WRAP_CONTENT));

        AssistantVerticalExpanderAccordion paymentRequestExpanderAccordion =
                new AssistantVerticalExpanderAccordion(mActivity, null);
        paymentRequestExpanderAccordion.setOrientation(LinearLayout.VERTICAL);
        paymentRequestExpanderAccordion.setLayoutParams(
                new LinearLayout.LayoutParams(/* width= */ ViewGroup.LayoutParams.MATCH_PARENT,
                        /* height= */ 0, /* weight= */ 1));
        paymentRequestExpanderAccordion.setOnExpandedViewChangedListener(
                expander -> mModel.set(AssistantPaymentRequestModel.EXPANDED_SECTION, expander));

        mPaymentRequestUI.addView(paymentRequestExpanderAccordion,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        AssistantPaymentRequestContactDetailsSection contactDetailsSection =
                new AssistantPaymentRequestContactDetailsSection(
                        mActivity, paymentRequestExpanderAccordion);
        createSeparator(paymentRequestExpanderAccordion);
        AssistantPaymentRequestPaymentMethodSection paymentMethodSection =
                new AssistantPaymentRequestPaymentMethodSection(
                        mActivity, paymentRequestExpanderAccordion);
        createSeparator(paymentRequestExpanderAccordion);
        AssistantPaymentRequestShippingAddressSection shippingAddressSection =
                new AssistantPaymentRequestShippingAddressSection(
                        mActivity, paymentRequestExpanderAccordion);
        createSeparator(paymentRequestExpanderAccordion);
        AssistantPaymentRequestTermsSection termsSection = new AssistantPaymentRequestTermsSection(
                mActivity, paymentRequestExpanderAccordion, /* showAsSingleCheckbox= */ false);
        AssistantPaymentRequestTermsSection termsAsCheckboxSection =
                new AssistantPaymentRequestTermsSection(mActivity, paymentRequestExpanderAccordion,
                        /* showAsSingleCheckbox= */ true);

        paymentRequestExpanderAccordion.setTag(
                AssistantTagsForTesting.PAYMENT_REQUEST_ACCORDION_TAG);
        contactDetailsSection.getView().setTag(
                AssistantTagsForTesting.PAYMENT_REQUEST_CONTACT_DETAILS_SECTION_TAG);
        paymentMethodSection.getView().setTag(
                AssistantTagsForTesting.PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_TAG);
        shippingAddressSection.getView().setTag(
                AssistantTagsForTesting.PAYMENT_REQUEST_SHIPPING_ADDRESS_SECTION_TAG);

        // Bind view and mediator through the model.
        mViewHolder = new AssistantPaymentRequestBinder.ViewHolder(mPaymentRequestUI,
                paymentRequestExpanderAccordion, sectionToSectionPadding, contactDetailsSection,
                paymentMethodSection, shippingAddressSection, termsSection, termsAsCheckboxSection,
                DIVIDER_TAG, activity);
        AssistantPaymentRequestBinder binder = new AssistantPaymentRequestBinder();
        PropertyModelChangeProcessor.create(model, mViewHolder, binder);

        // View is initially invisible.
        model.set(AssistantPaymentRequestModel.VISIBLE, false);
    }

    public View getView() {
        return mPaymentRequestUI;
    }

    /**
     * Explicitly clean up.
     */
    public void destroy() {
        mViewHolder.destroy();
        mViewHolder = null;
    }

    private void createSeparator(ViewGroup parent) {
        View divider = LayoutInflater.from(mActivity).inflate(
                R.layout.autofill_assistant_payment_request_section_divider, parent, false);
        divider.setTag(DIVIDER_TAG);
        parent.addView(divider);
    }
}
