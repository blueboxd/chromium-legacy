// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.app.Activity;

import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;

/**
 * An interface implemented by the embedder that allows the Site Settings UI to access
 * embedder-specific logic.
 */
public interface SiteSettingsClient {
    /**
     * @return the ManagedPreferenceDelegate instance that should be used when rendering
     *         Preferences.
     */
    ManagedPreferenceDelegate getManagedPreferenceDelegate();

    /**
     * @see org.chromium.chrome.browser.help.HelpAndFeedback#show
     */
    void launchHelpAndFeedbackActivity(Activity currentActivity, String helpContext);
}
