// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.preferences.sync;

import android.content.Context;
import android.support.v7.preference.Preference;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.ui.UiUtils;

/**
 * A preference that displays the current sync account and status (enabled, error, needs passphrase,
 * etc).
 */
public class SyncPreference extends Preference {
    public SyncPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        updateSyncSummaryAndIcon();
    }

    /**
     * Updates the summary and icon for this preference to reflect the current state of syncing.
     */
    public void updateSyncSummaryAndIcon() {
        setSummary(SyncPreferenceUtils.getSyncStatusSummary(getContext()));

        if (SyncPreferenceUtils.showSyncErrorIcon(getContext())) {
            setIcon(UiUtils.getTintedDrawable(
                    getContext(), R.drawable.sync_error, R.color.default_red));
        } else {
            setIcon(UiUtils.getTintedDrawable(getContext(), R.drawable.permission_background_sync,
                    R.color.default_icon_color_blue));
        }
    }
}
