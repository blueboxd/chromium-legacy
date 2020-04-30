// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.app.Activity;
import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;

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
     * Launches a support page relevant to settings UI pages.
     *
     * @see org.chromium.chrome.browser.help.HelpAndFeedback#show
     */
    void launchSettingsHelpAndFeedbackActivity(Activity currentActivity);

    /**
     * Launches a support page related to protected content.
     *
     * @see org.chromium.chrome.browser.help.HelpAndFeedback#show
     */
    void launchProtectedContentHelpAndFeedbackActivity(Activity currentActivity);

    /**
     * @return The WebappSettingsClient that should be used when showing the Site Settings UI.
     */
    WebappSettingsClient getWebappSettingsClient();

    /**
     * @return The BrowserContextHandle that should be used to read and update settings.
     */
    BrowserContextHandle getBrowserContextHandle();

    /**
     * Asynchronously looks up the locally cached favicon image for the given URL, generating a
     * fallback if one isn't available.
     *
     * @param faviconUrl The URL of the page to get the favicon for. If a favicon for the full URL
     *     can't be found, the favicon for its host will be used as a fallback.
     * @param callback A callback that will be called with the favicon bitmap, or null if no
     *     favicon could be found or generated.
     */
    void getFaviconImageForURL(String faviconUrl, Callback<Bitmap> callback);

    /**
     * @return true if the QuietNotificationPrompts Feature is enabled.
     */
    boolean isQuietNotificationPromptsFeatureEnabled();

    /**
     * @return The SiteSettingsPrefClient that should be used to access native prefs when rendering
     *     the SiteSettings UI.
     */
    // TODO(crbug.com/1071603): Remove this once PrefServiceBridge is componentized.
    SiteSettingsPrefClient getSiteSettingsPrefClient();

    /**
     * @return The id of the notification channel associated with the given origin.
     */
    // TODO(crbug.com/1069895): Remove this once WebLayer supports notifications.
    String getChannelIdForOrigin(String origin);
}
