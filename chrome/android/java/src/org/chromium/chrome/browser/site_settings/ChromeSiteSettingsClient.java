// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.app.Activity;
import android.graphics.Bitmap;

import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;

import java.util.Set;

/**
 * A SiteSettingsClient instance that contains Chrome-specific Site Settings logic.
 */
public class ChromeSiteSettingsClient implements SiteSettingsClient {
    private ChromeNotificationSettingsClient mChromeNotificationSettingsClient;
    private ChromeSiteSettingsPrefClient mChromeSiteSettingsPrefClient;
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

    @Override
    public void launchSettingsHelpAndFeedbackActivity(Activity currentActivity) {
        HelpAndFeedback.getInstance().show(currentActivity,
                currentActivity.getString(R.string.help_context_settings),
                Profile.getLastUsedRegularProfile(), null);
    }

    @Override
    public void launchProtectedContentHelpAndFeedbackActivity(Activity currentActivity) {
        HelpAndFeedback.getInstance().show(currentActivity,
                currentActivity.getString(R.string.help_context_protected_content),
                Profile.getLastUsedRegularProfile(), null);
    }

    @Override
    public NotificationSettingsClient getNotificationSettingsClient() {
        if (mChromeNotificationSettingsClient == null) {
            mChromeNotificationSettingsClient = new ChromeNotificationSettingsClient();
        }
        return mChromeNotificationSettingsClient;
    }

    @Override
    public BrowserContextHandle getBrowserContextHandle() {
        return Profile.getLastUsedRegularProfile();
    }

    @Override
    public void getLocalFaviconImageForURL(
            String faviconUrl, int faviconSizePx, Callback<Bitmap> callback) {
        new FaviconLoader(faviconUrl, faviconSizePx, callback);
    }

    /**
     * A helper class that groups a FaviconHelper with its corresponding Callback.
     *
     * This object is kept alive by being passed to the native
     * FaviconHelper.getLocalFaviconImageForURL. Its reference will be released after the callback
     * has been called.
     */
    private static class FaviconLoader implements FaviconImageCallback {
        // Loads the favicons asynchronously.
        private final FaviconHelper mFaviconHelper;
        private final Callback<Bitmap> mCallback;

        private FaviconLoader(String faviconUrl, int faviconSizePx, Callback<Bitmap> callback) {
            mCallback = callback;
            mFaviconHelper = new FaviconHelper();

            // TODO(https://crbug.com/1048632): Use the current profile (i.e., regular profile or
            // incognito profile) instead of always using regular profile. It works correctly now,
            // but it is not safe.
            if (!mFaviconHelper.getLocalFaviconImageForURL(
                        Profile.getLastUsedRegularProfile(), faviconUrl, faviconSizePx, this)) {
                onFaviconAvailable(/*image=*/null, faviconUrl);
            }
        }

        @Override
        public void onFaviconAvailable(Bitmap image, String iconUrl) {
            mFaviconHelper.destroy();
            mCallback.onResult(image);
        }
    }

    @Override
    public boolean isQuietNotificationPromptsFeatureEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.QUIET_NOTIFICATION_PROMPTS);
    }

    @Override
    public ChromeSiteSettingsPrefClient getSiteSettingsPrefClient() {
        if (mChromeSiteSettingsPrefClient == null) {
            mChromeSiteSettingsPrefClient = new ChromeSiteSettingsPrefClient();
        }
        return mChromeSiteSettingsPrefClient;
    }

    @Override
    public boolean originHasInstalledWebapp(String origin) {
        WebappRegistry registry = WebappRegistry.getInstance();
        Set<String> originsWithApps = registry.getOriginsWithInstalledApp();
        return originsWithApps.contains(origin);
    }
}
