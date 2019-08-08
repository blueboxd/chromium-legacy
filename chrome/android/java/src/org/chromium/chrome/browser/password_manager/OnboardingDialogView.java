// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * The dialog content view for the password manager onboarding dialog.
 */
public class OnboardingDialogView extends LinearLayout {
    // View containing the illustration for the onboarding.
    private ImageView mOnboardingIllustrationView;
    // View containing the header of the onboarding.
    private TextView mOnboardingTitleView;
    // View containing the explanation text.
    private TextView mOnboardingDetailsView;

    public OnboardingDialogView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mOnboardingIllustrationView = findViewById(R.id.onboarding_illustration);
        mOnboardingTitleView = findViewById(R.id.onboarding_title);
        mOnboardingDetailsView = findViewById(R.id.onboarding_details);
    }

    void setOnboardingIllustration(int onboardingIllustration) {
        mOnboardingIllustrationView.setImageResource(onboardingIllustration);
    }

    void setOnboardingTitle(String onboardingTitle) {
        mOnboardingTitleView.setText(onboardingTitle);
    }

    void setOnboardingDetails(String onboardingDetails) {
        mOnboardingDetailsView.setText(onboardingDetails);
    }
}
