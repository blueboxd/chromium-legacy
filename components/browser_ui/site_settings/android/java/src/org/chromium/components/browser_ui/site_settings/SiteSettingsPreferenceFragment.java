// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.preference.PreferenceFragmentCompat;

/**
 * Preference fragment for showing the Site Settings UI.
 */
public abstract class SiteSettingsPreferenceFragment extends PreferenceFragmentCompat {
    private SiteSettingsClient mSiteSettingsClient;

    /**
     * Sets the SiteSettingsClient instance this Fragment should use.
     *
     * This should be called by the embedding Activity.
     */
    public void setSiteSettingsClient(SiteSettingsClient client) {
        assert mSiteSettingsClient == null;
        mSiteSettingsClient = client;
    }

    /**
     * @return the SiteSettingsClient instance to use when rendering the Site Settings UI.
     */
    protected SiteSettingsClient getSiteSettingsClient() {
        assert mSiteSettingsClient != null : "SiteSettingsClient not set";
        return mSiteSettingsClient;
    }
}
