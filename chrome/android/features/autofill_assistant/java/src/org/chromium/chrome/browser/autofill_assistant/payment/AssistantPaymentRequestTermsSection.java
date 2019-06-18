// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.content.Context;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.ui.text.SpanApplier;

/**
 * The third party terms and conditions section of the Autofill Assistant payment request.
 */
public class AssistantPaymentRequestTermsSection {
    private final View mView;
    private final AssistantChoiceList mTermsList;
    private final TextView mTermsAgree;
    private final TextView mTermsRequiresReview;
    private final TextView mThirdPartyPrivacyNotice;
    private Callback<Integer> mListener;

    AssistantPaymentRequestTermsSection(Context context, ViewGroup parent) {
        mView = LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_payment_request_terms_and_conditions, parent, false);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        parent.addView(mView, lp);
        mTermsList = mView.findViewById(R.id.third_party_terms_list);

        mTermsAgree = new TextView(context);
        ApiCompatibilityUtils.setTextAppearance(mTermsAgree, R.style.TextAppearance_BlackCaption);
        mTermsList.addItem(mTermsAgree, /*hasEditButton=*/false, selected -> {
            if (selected && mListener != null) {
                mListener.onResult(AssistantTermsAndConditionsState.ACCEPTED);
            }
        }, /* itemEditedListener=*/null);

        mTermsRequiresReview = new TextView(context);
        ApiCompatibilityUtils.setTextAppearance(
                mTermsRequiresReview, R.style.TextAppearance_BlackCaption);
        mTermsList.addItem(mTermsRequiresReview, /*hasEditButton=*/false, selected -> {
            if (selected && mListener != null) {
                mListener.onResult(AssistantTermsAndConditionsState.REQUIRES_REVIEW);
            }
        }, /* itemEditedListener=*/null);

        mThirdPartyPrivacyNotice =
                mView.findViewById(R.id.payment_request_3rd_party_privacy_notice);
    }

    public void setOrigin(String origin) {
        StyleSpan boldSpan = new StyleSpan(android.graphics.Typeface.BOLD);
        Context context = mView.getContext();
        mTermsAgree.setText(SpanApplier.applySpans(
                context.getString(R.string.autofill_assistant_3rd_party_terms_accept, origin),
                new SpanApplier.SpanInfo("<b>", "</b>", boldSpan)));

        mTermsRequiresReview.setText(SpanApplier.applySpans(
                context.getString(R.string.autofill_assistant_3rd_party_terms_review, origin),
                new SpanApplier.SpanInfo("<b>", "</b>", boldSpan)));

        mThirdPartyPrivacyNotice.setText(SpanApplier.applySpans(
                context.getString(R.string.autofill_assistant_3rd_party_privacy_notice, origin),
                new SpanApplier.SpanInfo("<b>", "</b>", boldSpan)));
    }

    public void setTermsStatus(@AssistantTermsAndConditionsState int status) {
        switch (status) {
            case AssistantTermsAndConditionsState.NOT_SELECTED:
                mTermsList.setCheckedItem(null);
                break;
            case AssistantTermsAndConditionsState.ACCEPTED:
                mTermsList.setCheckedItem(mTermsAgree);
                break;
            case AssistantTermsAndConditionsState.REQUIRES_REVIEW:
                mTermsList.setCheckedItem(mTermsRequiresReview);
                break;
        }
    }

    public void setPaddings(int topPadding, int bottomPadding) {
        mView.setPadding(
                mView.getPaddingLeft(), topPadding, mView.getPaddingRight(), bottomPadding);
    }

    public void setListener(Callback<Integer> listener) {
        mListener = listener;
    }

    public void setTermsListVisible(boolean visible) {
        mTermsList.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    View getView() {
        return mView;
    }
}
