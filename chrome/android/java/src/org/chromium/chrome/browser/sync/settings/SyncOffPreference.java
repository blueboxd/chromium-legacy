// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.DialogPreference;

import org.chromium.chrome.R;

/**
 * A preference that displays "Sign out and turn off sync" button in {@link ManageSyncSettings}.
 */
public class SyncOffPreference extends DialogPreference {
    public SyncOffPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.preference_turn_off_sync);
    }
}
