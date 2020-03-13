// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;

/**
 * A 4-state radio group Preference used for the Cookies subpage of SiteSettings.
 */
public class FourStateCookieSettingsPreference
        extends Preference implements RadioGroup.OnCheckedChangeListener {
    public enum CookieSettingsState {
        UNINITIALIZED,
        ALLOW,
        BLOCK_THIRD_PARTY_INCOGNITO,
        BLOCK_THIRD_PARTY,
        BLOCK
    }

    private CookieSettingsState mState = CookieSettingsState.UNINITIALIZED;
    private RadioButtonWithDescription mAllow;
    private RadioButtonWithDescription mBlockThirdPartyIncognito;
    private RadioButtonWithDescription mBlockThirdParty;
    private RadioButtonWithDescription mBlock;
    private RadioGroup mRadioGroup;

    public FourStateCookieSettingsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Sets the layout resource that will be inflated for the view.
        setLayoutResource(R.layout.four_state_cookie_settings_preference);

        // Make unselectable, otherwise FourStateCookieSettingsPreference is treated as one
        // selectable Preference, instead of four selectable radio buttons.
        setSelectable(false);
    }

    /**
     * Sets the state of this Preference.
     * @param state The CookieSettingsState to set the preference to.
     */
    public void setState(CookieSettingsState state) {
        mState = state;

        if (mRadioGroup != null) {
            setRadioButton();
        }
    }

    /**
     * @return The state that is currently selected.
     */
    public CookieSettingsState getState() {
        return mState;
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        if (mAllow.isChecked()) {
            mState = CookieSettingsState.ALLOW;
        } else if (mBlockThirdPartyIncognito.isChecked()) {
            mState = CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO;
        } else if (mBlockThirdParty.isChecked()) {
            mState = CookieSettingsState.BLOCK_THIRD_PARTY;
        } else if (mBlock.isChecked()) {
            mState = CookieSettingsState.BLOCK;
        }

        callChangeListener(mState);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mAllow = (RadioButtonWithDescription) holder.findViewById(R.id.allow);
        mBlockThirdPartyIncognito =
                (RadioButtonWithDescription) holder.findViewById(R.id.block_third_party_incognito);
        mBlockThirdParty = (RadioButtonWithDescription) holder.findViewById(R.id.block_third_party);
        mBlock = (RadioButtonWithDescription) holder.findViewById(R.id.block);
        mRadioGroup = (RadioGroup) holder.findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(this);

        if (mState != CookieSettingsState.UNINITIALIZED) {
            setRadioButton();
        }
    }

    private void setRadioButton() {
        RadioButtonWithDescription button = null;

        switch (mState) {
            case ALLOW:
                button = mAllow;
                break;
            case BLOCK_THIRD_PARTY_INCOGNITO:
                button = mBlockThirdPartyIncognito;
                break;
            case BLOCK_THIRD_PARTY:
                button = mBlockThirdParty;
                break;
            case BLOCK:
                button = mBlock;
                break;
            default:
                return;
        }

        button.setChecked(true);
    }
}
