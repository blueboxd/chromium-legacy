// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.support.v7.preference.CheckBoxPreference;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;
import android.widget.TextView;

/**
 * Contains the basic functionality that should be shared by all CheckBoxPreference in Chrome.
 */
public class ChromeBaseCheckBoxPreferenceCompat extends CheckBoxPreference {
    private ManagedPreferenceDelegateCompat mManagedPrefDelegate;

    /**
     * Constructor for inflating from XML.
     */
    public ChromeBaseCheckBoxPreferenceCompat(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the ManagedPreferenceDelegate which will determine whether this preference is managed.
     */
    public void setManagedPreferenceDelegate(ManagedPreferenceDelegateCompat delegate) {
        mManagedPrefDelegate = delegate;
        ManagedPreferencesUtils.initPreference(mManagedPrefDelegate, this);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        ((TextView) holder.findViewById(android.R.id.title)).setSingleLine(false);
        ManagedPreferencesUtils.onBindViewToPreference(mManagedPrefDelegate, this, holder.itemView);
    }

    @Override
    protected void onClick() {
        if (ManagedPreferencesUtils.onClickPreference(mManagedPrefDelegate, this)) return;
        super.onClick();
    }
}
