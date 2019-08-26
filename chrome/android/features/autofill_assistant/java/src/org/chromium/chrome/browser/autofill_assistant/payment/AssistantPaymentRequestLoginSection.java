// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import java.util.List;

/**
 * The login details section of the Autofill Assistant payment request.
 */
public class AssistantPaymentRequestLoginSection
        extends AssistantPaymentRequestSection<AssistantPaymentRequestLoginChoice> {
    AssistantPaymentRequestLoginSection(Context context, ViewGroup parent) {
        super(context, parent,
                org.chromium.chrome.autofill_assistant.R.layout.autofill_assistant_login,
                org.chromium.chrome.autofill_assistant.R.layout.autofill_assistant_login,
                context.getResources().getDimensionPixelSize(
                        org.chromium.chrome.autofill_assistant.R.dimen
                                .autofill_assistant_payment_request_title_padding),
                /*titleAddButton=*/null, /*listAddButton=*/null, /*canEditItems=*/false);
    }

    @Override
    protected void createOrEditItem(@Nullable AssistantPaymentRequestLoginChoice oldItem) {
        // Nothing to do, this section currently does not support adding or creating items.
    }

    @Override
    protected void updateFullView(View fullView, AssistantPaymentRequestLoginChoice option) {
        updateSummaryView(fullView, option);
    }

    @Override
    protected void updateSummaryView(View summaryView, AssistantPaymentRequestLoginChoice option) {
        TextView usernameView =
                summaryView.findViewById(org.chromium.chrome.autofill_assistant.R.id.username);
        usernameView.setText(option.getLabel());
    }

    /**
     * The login options have changed externally. This will rebuild the UI with the new/changed
     * set of login options, while keeping the selected item if possible.
     */
    void onLoginsChanged(List<AssistantPaymentRequestLoginChoice> options) {
        int indexToSelect = -1;
        if (mSelectedOption != null) {
            for (int i = 0; i < getItems().size(); i++) {
                if (TextUtils.equals(
                            mSelectedOption.getIdentifier(), getItems().get(i).getIdentifier())) {
                    indexToSelect = i;
                    break;
                }
            }
        }

        // Preselect login option according to priority. This will implicitly select the first
        // option if all options have the same (default) priority.
        if (indexToSelect == -1) {
            int highestPriority = Integer.MAX_VALUE;
            for (int i = 0; i < options.size(); i++) {
                int priority = options.get(i).getPriority();
                if (priority < highestPriority) {
                    highestPriority = priority;
                    indexToSelect = i;
                }
            }
        }

        setItems(options, indexToSelect);
    }
}
