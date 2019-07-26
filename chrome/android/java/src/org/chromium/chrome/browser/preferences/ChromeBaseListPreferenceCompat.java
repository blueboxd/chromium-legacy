// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.support.v7.preference.ListPreference;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;

/**
 * Contains the basic functionality that should be shared by all ListPreference in Chrome.
 */
public class ChromeBaseListPreferenceCompat extends ListPreference {
    private ManagedPreferenceDelegateCompat mManagedPrefDelegate;

    /**
     * Constructor for inflating from XML.
     */
    public ChromeBaseListPreferenceCompat(Context context, AttributeSet attrs) {
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
        setSingleLineTitle(false);
        ManagedPreferencesUtils.onBindViewToPreference(mManagedPrefDelegate, this, holder.itemView);
    }

    @Override
    protected void onClick() {
        if (ManagedPreferencesUtils.onClickPreference(mManagedPrefDelegate, this)) return;
        super.onClick();
    }
}
