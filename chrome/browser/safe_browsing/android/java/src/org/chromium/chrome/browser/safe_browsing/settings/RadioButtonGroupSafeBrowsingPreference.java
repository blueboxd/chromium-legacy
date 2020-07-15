// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RadioGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

/**
 * A radio button group used for Safe Browsing. Currently, it has 3 options: Enhanced Protection,
 * Standard Protection and No Protection. When the Enhanced Protection flag is disabled, the
 * Enhanced Protection option will be removed.
 */
public class RadioButtonGroupSafeBrowsingPreference
        extends Preference implements RadioGroup.OnCheckedChangeListener {
    private @Nullable RadioButtonWithDescriptionAndAuxButton mEnhancedProtection;
    private RadioButtonWithDescriptionAndAuxButton mStandardProtection;
    private RadioButtonWithDescription mNoProtection;
    private @SafeBrowsingState int mSafeBrowsingState;
    private boolean mIsEnhancedProtectionEnabled;

    public RadioButtonGroupSafeBrowsingPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.radio_button_group_safe_browsing_preference);
    }

    /**
     * Set Safe Browsing state and Enhanced Protection state. Called before onBindViewHolder.
     * @param safeBrowsingState The current Safe Browsing state.
     * @param isEnhancedProtectionEnabled Whether to show the Enhanced Protection button.
     */
    public void init(
            @SafeBrowsingState int safeBrowsingState, boolean isEnhancedProtectionEnabled) {
        mSafeBrowsingState = safeBrowsingState;
        mIsEnhancedProtectionEnabled = isEnhancedProtectionEnabled;
        assert ((mSafeBrowsingState != SafeBrowsingState.ENHANCED_PROTECTION)
                || mIsEnhancedProtectionEnabled)
            : "Safe Browsing state shouldn't be enhanced protection when the flag is disabled.";
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        if (mIsEnhancedProtectionEnabled && checkedId == mEnhancedProtection.getId()) {
            mSafeBrowsingState = SafeBrowsingState.ENHANCED_PROTECTION;
        } else if (checkedId == mStandardProtection.getId()) {
            mSafeBrowsingState = SafeBrowsingState.STANDARD_PROTECTION;
        } else if (checkedId == mNoProtection.getId()) {
            mSafeBrowsingState = SafeBrowsingState.NO_SAFE_BROWSING;
        } else {
            assert false : "Should not be reached.";
        }
        callChangeListener(mSafeBrowsingState);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        if (mIsEnhancedProtectionEnabled) {
            mEnhancedProtection = (RadioButtonWithDescriptionAndAuxButton) holder.findViewById(
                    R.id.enhanced_protection);
            mEnhancedProtection.setVisibility(View.VISIBLE);
        }
        mStandardProtection = (RadioButtonWithDescriptionAndAuxButton) holder.findViewById(
                R.id.standard_protection);
        mNoProtection = (RadioButtonWithDescription) holder.findViewById(R.id.no_protection);
        RadioButtonWithDescriptionLayout groupLayout =
                (RadioButtonWithDescriptionLayout) mNoProtection.getRootView();
        groupLayout.setOnCheckedChangeListener(this);

        assert ((mSafeBrowsingState != SafeBrowsingState.ENHANCED_PROTECTION)
                || mIsEnhancedProtectionEnabled)
            : "Safe Browsing state shouldn't be enhanced protection when the flag is disabled.";
        if (mIsEnhancedProtectionEnabled) {
            mEnhancedProtection.setChecked(
                    mSafeBrowsingState == SafeBrowsingState.ENHANCED_PROTECTION);
        }
        mStandardProtection.setChecked(mSafeBrowsingState == SafeBrowsingState.STANDARD_PROTECTION);
        mNoProtection.setChecked(mSafeBrowsingState == SafeBrowsingState.NO_SAFE_BROWSING);
    }

    @VisibleForTesting
    public @SafeBrowsingState int getSafeBrowsingStateForTesting() {
        return mSafeBrowsingState;
    }

    @VisibleForTesting
    public RadioButtonWithDescriptionAndAuxButton getEnhancedProtectionButtonForTesting() {
        return mEnhancedProtection;
    }

    @VisibleForTesting
    public RadioButtonWithDescriptionAndAuxButton getStandardProtectionButtonForTesting() {
        return mStandardProtection;
    }

    @VisibleForTesting
    public RadioButtonWithDescription getNoProtectionButtonForTesting() {
        return mNoProtection;
    }
}
