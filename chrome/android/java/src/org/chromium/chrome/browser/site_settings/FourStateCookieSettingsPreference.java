// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RadioGroup;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.components.content_settings.CookieControlsMode;

import java.util.Arrays;

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

    // Signals used to determine the view and button states.
    private boolean mCookiesContentSettingEnforced;
    private boolean mThirdPartyBlockingEnforced;
    private boolean mAllowCookies;
    private boolean mBlockThirdPartyCookies;
    // An enum indicating when to block third-party cookies.
    private @CookieControlsMode int mCookieControlsMode;
    private CookieSettingsState mState = CookieSettingsState.UNINITIALIZED;

    // UI Elements.
    private RadioButtonWithDescription mAllowButton;
    private RadioButtonWithDescription mBlockThirdPartyIncognitoButton;
    private RadioButtonWithDescription mBlockThirdPartyButton;
    private RadioButtonWithDescription mBlockButton;
    private RadioGroup mRadioGroup;
    private TextViewWithCompoundDrawables mManagedView;

    public FourStateCookieSettingsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Sets the layout resource that will be inflated for the view.
        setLayoutResource(R.layout.four_state_cookie_settings_preference);

        // Make unselectable, otherwise FourStateCookieSettingsPreference is treated as one
        // selectable Preference, instead of four selectable radio buttons.
        setSelectable(false);
    }

    /**
     * Sets the cookie settings state and updates the radio buttons.
     * @param cookiesContentSettingEnforced Whether the cookies content setting is enforced.
     * @param thirdPartyBlockingEnforced Whether third-party blocking is enforced.
     * @param allowCookies Whether the cookies content setting is enabled.
     * @param blockThirdPartyCookies Whether third-party blocking is enabled.
     * @param cookieControlsMode The CookieControlsMode enum for the current cookie settings state.
     */
    public void setState(boolean cookiesContentSettingEnforced, boolean thirdPartyBlockingEnforced,
            boolean allowCookies, boolean blockThirdPartyCookies,
            @CookieControlsMode int cookieControlsMode) {
        mCookiesContentSettingEnforced = cookiesContentSettingEnforced;
        mThirdPartyBlockingEnforced = thirdPartyBlockingEnforced;
        mAllowCookies = allowCookies;
        mBlockThirdPartyCookies = blockThirdPartyCookies;
        mCookieControlsMode = cookieControlsMode;

        if (mRadioGroup != null) {
            setRadioButtons();
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
        if (mAllowButton.isChecked()) {
            mState = CookieSettingsState.ALLOW;
        } else if (mBlockThirdPartyIncognitoButton.isChecked()) {
            mState = CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO;
        } else if (mBlockThirdPartyButton.isChecked()) {
            mState = CookieSettingsState.BLOCK_THIRD_PARTY;
        } else if (mBlockButton.isChecked()) {
            mState = CookieSettingsState.BLOCK;
        }

        callChangeListener(mState);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mAllowButton = (RadioButtonWithDescription) holder.findViewById(R.id.allow);
        mBlockThirdPartyIncognitoButton =
                (RadioButtonWithDescription) holder.findViewById(R.id.block_third_party_incognito);
        mBlockThirdPartyButton =
                (RadioButtonWithDescription) holder.findViewById(R.id.block_third_party);
        mBlockButton = (RadioButtonWithDescription) holder.findViewById(R.id.block);
        mRadioGroup = (RadioGroup) holder.findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(this);

        mManagedView = (TextViewWithCompoundDrawables) holder.findViewById(R.id.managed_view);
        Drawable[] drawables = mManagedView.getCompoundDrawablesRelative();
        Drawable managedIcon = ApiCompatibilityUtils.getDrawable(getContext().getResources(),
                ManagedPreferencesUtils.getManagedByEnterpriseIconId());
        mManagedView.setCompoundDrawablesRelativeWithIntrinsicBounds(
                managedIcon, drawables[1], drawables[2], drawables[3]);

        setRadioButtons();
    }

    private RadioButtonWithDescription getActiveRadioButton() {
        // These conditions only check the preference combinations that deterministically decide
        // your cookie settings state. In the future we would refactor the backend preferences to
        // reflect the only possible states you can be in
        // (Allow/BlockThirdPartyIncognito/BlockThirdParty/Block), instead of using this
        // combination of multiple signals.
        if (!mAllowCookies) {
            mState = CookieSettingsState.BLOCK;
            return mBlockButton;
        } else if (mBlockThirdPartyCookies || mCookieControlsMode == CookieControlsMode.ON) {
            // Having CookieControlsMode.ON is equivalent to having the BLOCK_THIRD_PARTY_COOKIES
            // pref set to enabled, because it means third party cookie blocking is always on.
            mState = CookieSettingsState.BLOCK_THIRD_PARTY;
            return mBlockThirdPartyButton;
        } else if (mCookieControlsMode == CookieControlsMode.INCOGNITO_ONLY) {
            mState = CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO;
            return mBlockThirdPartyIncognitoButton;
        } else {
            mState = CookieSettingsState.ALLOW;
            return mAllowButton;
        }
    }

    private void setRadioButtons() {
        mAllowButton.setEnabled(true);
        mBlockThirdPartyIncognitoButton.setEnabled(true);
        mBlockThirdPartyButton.setEnabled(true);
        mBlockButton.setEnabled(true);
        for (RadioButtonWithDescription button : getEnforcedButtons()) {
            button.setEnabled(false);
        }
        mManagedView.setVisibility((mCookiesContentSettingEnforced || mThirdPartyBlockingEnforced)
                        ? View.VISIBLE
                        : View.GONE);

        RadioButtonWithDescription button = getActiveRadioButton();
        // Always want to enable the selected option.
        button.setEnabled(true);
        button.setChecked(true);
    }

    /**
     * A helper function to return a button array from a variable number of arguments.
     */
    private RadioButtonWithDescription[] buttons(RadioButtonWithDescription... args) {
        return args;
    }

    /**
     * @return An array of radio buttons that have to be disabled as they can't be selected due to
     *         policy restrictions.
     */
    private RadioButtonWithDescription[] getEnforcedButtons() {
        if (!mCookiesContentSettingEnforced && !mThirdPartyBlockingEnforced) {
            return buttons();
        }
        if (mCookiesContentSettingEnforced && mThirdPartyBlockingEnforced) {
            return buttons(mAllowButton, mBlockThirdPartyIncognitoButton, mBlockThirdPartyButton,
                    mBlockButton);
        }
        if (mCookiesContentSettingEnforced) {
            if (mAllowCookies) {
                return buttons(mBlockButton);
            } else {
                return buttons(mAllowButton, mBlockThirdPartyIncognitoButton,
                        mBlockThirdPartyButton, mBlockButton);
            }
        }
        if (mBlockThirdPartyCookies) {
            return buttons(mAllowButton, mBlockThirdPartyIncognitoButton);
        } else {
            return buttons(mBlockThirdPartyIncognitoButton, mBlockThirdPartyButton);
        }
    }

    @VisibleForTesting
    public boolean isStateEnforced(CookieSettingsState state) {
        RadioButtonWithDescription button;
        switch (state) {
            case ALLOW:
                button = mAllowButton;
                break;
            case BLOCK_THIRD_PARTY_INCOGNITO:
                button = mBlockThirdPartyIncognitoButton;
                break;
            case BLOCK_THIRD_PARTY:
                button = mBlockThirdPartyButton;
                break;
            case BLOCK:
                button = mBlockButton;
                break;
            default:
                return false;
        }
        return Arrays.asList(getEnforcedButtons()).contains(button);
    }
}
