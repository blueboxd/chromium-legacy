// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import androidx.preference.Preference;

import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;

/**
 * A SiteSettingsClient instance that contains Chrome-specific Site Settings logic.
 */
public class ChromeSiteSettingsClient implements SiteSettingsClient {
    private ManagedPreferenceDelegate mManagedPreferenceDelegate;

    @Override
    public ManagedPreferenceDelegate getManagedPreferenceDelegate() {
        if (mManagedPreferenceDelegate == null) {
            mManagedPreferenceDelegate = new ChromeManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return false;
                }
            };
        }
        return mManagedPreferenceDelegate;
    }
}
