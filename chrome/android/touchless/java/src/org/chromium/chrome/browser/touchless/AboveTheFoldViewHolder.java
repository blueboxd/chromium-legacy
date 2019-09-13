// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

/**
 * Custom ViewHolder for the last tab button and site suggestions carousel. Handles restore focus
 * for those components.
 */
public class AboveTheFoldViewHolder extends NewTabPageViewHolder {
    private static final String FOCUS_TYPE_KEY = "FTK";

    @IntDef({FocusType.UNKNOWN, FocusType.LAST_TAB_BUTTON, FocusType.SITE_SUGGESTIONS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface FocusType {
        int UNKNOWN = 0;
        int LAST_TAB_BUTTON = 1;
        int SITE_SUGGESTIONS = 2;
    }

    private final Map<Integer, FocusableComponent> mComponents;

    public AboveTheFoldViewHolder(View itemView, FocusableComponent focusableOpenLastTab,
            FocusableComponent focusableSiteSuggestions) {
        super(itemView);
        mComponents = new HashMap<>();
        mComponents.put(FocusType.LAST_TAB_BUTTON, focusableOpenLastTab);
        mComponents.put(FocusType.SITE_SUGGESTIONS, focusableSiteSuggestions);
    }

    @Override
    public void requestFocusWithBundle(Bundle focusBundle) {
        @FocusType
        int type = focusBundle.getInt(FOCUS_TYPE_KEY);
        if (mComponents.containsKey(type)) {
            mComponents.get(type).requestFocus();
        }
    }

    @Override
    public void setOnFocusListener(Callback<Bundle> callback) {
        for (Map.Entry<Integer, FocusableComponent> kv : mComponents.entrySet()) {
            kv.getValue().setOnFocusListener(() -> {
                Bundle focusBundle = new Bundle();
                focusBundle.putInt(FOCUS_TYPE_KEY, kv.getKey());
                callback.onResult(focusBundle);
            });
        }
    }
}
