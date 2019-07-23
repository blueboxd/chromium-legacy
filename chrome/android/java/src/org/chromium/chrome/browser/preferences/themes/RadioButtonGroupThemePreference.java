// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.themes;

import android.content.Context;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;

import org.chromium.base.BuildInfo;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.night_mode.NightModeMetrics;
import org.chromium.chrome.browser.preferences.themes.ThemePreferences.ThemeSetting;
import org.chromium.chrome.browser.widget.RadioButtonWithDescription;

import java.util.ArrayList;
import java.util.Collections;

/**
 * A radio button group Preference used for Themes. Currently, it has 3 options: System default,
 * Light, and Dark.
 */
public class RadioButtonGroupThemePreference
        extends Preference implements RadioButtonWithDescription.OnCheckedChangeListener {
    private @ThemeSetting int mSetting;
    private ArrayList<RadioButtonWithDescription> mButtons;

    public RadioButtonGroupThemePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.radio_button_group_theme_preference);

        // Initialize entries with null objects so that calling ArrayList#set() would not throw
        // java.lang.IndexOutOfBoundsException.
        mButtons = new ArrayList<>(Collections.nCopies(ThemeSetting.NUM_ENTRIES, null));
    }

    /**
     * @param setting The initial setting for this Preference
     */
    public void initialize(@ThemeSetting int setting) {
        mSetting = setting;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        assert ThemeSetting.NUM_ENTRIES == 3;
        mButtons.set(ThemeSetting.SYSTEM_DEFAULT,
                (RadioButtonWithDescription) holder.findViewById(R.id.system_default));
        if (BuildInfo.isAtLeastQ()) {
            mButtons.get(ThemeSetting.SYSTEM_DEFAULT)
                    .setDescriptionText(
                            getContext().getString(R.string.themes_system_default_summary_api_29));
        }
        mButtons.set(
                ThemeSetting.LIGHT, (RadioButtonWithDescription) holder.findViewById(R.id.light));
        mButtons.set(
                ThemeSetting.DARK, (RadioButtonWithDescription) holder.findViewById(R.id.dark));

        for (int i = 0; i < ThemeSetting.NUM_ENTRIES; i++) {
            mButtons.get(i).setRadioButtonGroup(mButtons);
            mButtons.get(i).setOnCheckedChangeListener(this);
        }

        mButtons.get(mSetting).setChecked(true);
    }

    @Override
    public void onCheckedChanged() {
        for (int i = 0; i < ThemeSetting.NUM_ENTRIES; i++) {
            if (mButtons.get(i).isChecked()) {
                mSetting = i;
                break;
            }
        }
        callChangeListener(mSetting);
        NightModeMetrics.recordThemePreferencesChanged(mSetting);
    }

    @VisibleForTesting
    public int getSetting() {
        return mSetting;
    }

    @VisibleForTesting
    ArrayList getButtonsForTesting() {
        return mButtons;
    }
}
