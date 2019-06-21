// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.Arrays;

/**
 * Coordinator responsible for showing the onboarding screen when the user is using the Autofill
 * Assistant for the first time.
 */
class AssistantOnboardingCoordinator {
    private static final String SMALL_ONBOARDING_EXPERIMENT_ID = "4257013";

    /**
     * Set the content of {@code bottomSheetContent} to be the Autofill Assistant onboarding. {@code
     * callback} will be called with true or false when the user accepts or cancels the onboarding
     * (respectively).
     */
    static void setOnboardingContent(String experimentIds, Context context,
            AssistantBottomSheetContent bottomSheetContent, Callback<Boolean> callback) {
        ScrollView initView = (ScrollView) LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_onboarding, /* root= */ null);

        TextView termsTextView = initView.findViewById(R.id.google_terms_message);
        String termsString = context.getApplicationContext().getString(
                R.string.autofill_assistant_google_terms_description);

        NoUnderlineClickableSpan termsSpan = new NoUnderlineClickableSpan(context.getResources(),
                (widget)
                        -> CustomTabActivity.showInfoPage(context.getApplicationContext(),
                                context.getApplicationContext().getString(
                                        R.string.autofill_assistant_google_terms_url)));
        SpannableString spannableMessage = SpanApplier.applySpans(
                termsString, new SpanApplier.SpanInfo("<link>", "</link>", termsSpan));
        termsTextView.setText(spannableMessage);
        termsTextView.setMovementMethod(LinkMovementMethod.getInstance());

        // Set focusable for accessibility.
        initView.setFocusable(true);

        initView.findViewById(R.id.button_init_ok)
                .setOnClickListener(unusedView -> onClicked(true, callback));
        initView.findViewById(R.id.button_init_not_ok)
                .setOnClickListener(unusedView -> onClicked(false, callback));
        initView.announceForAccessibility(
                context.getString(R.string.autofill_assistant_first_run_accessibility));

        // Hide views that should not be displayed when showing the small onboarding.
        if (Arrays.asList(experimentIds.split(",")).contains(SMALL_ONBOARDING_EXPERIMENT_ID)) {
            hide(initView, R.id.onboarding_image);
            hide(initView, R.id.onboarding_subtitle);
            hide(initView, R.id.onboarding_separator);
        }

        bottomSheetContent.setContent(initView, initView);
    }

    private static void hide(View root, int resId) {
        root.findViewById(resId).setVisibility(View.GONE);
    }

    private static void onClicked(boolean accept, Callback<Boolean> callback) {
        AutofillAssistantPreferencesUtil.setInitialPreferences(accept);
        callback.onResult(accept);
    }
}
