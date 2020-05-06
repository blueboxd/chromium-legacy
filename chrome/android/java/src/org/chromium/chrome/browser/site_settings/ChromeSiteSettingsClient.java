// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;

import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.SiteSettingsClient;
import org.chromium.components.browser_ui.site_settings.SiteSettingsHelpClient;
import org.chromium.components.browser_ui.site_settings.WebappSettingsClient;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;

/**
 * A SiteSettingsClient instance that contains Chrome-specific Site Settings logic.
 */
public class ChromeSiteSettingsClient implements SiteSettingsClient {
    // Constants for favicon processing.
    // TODO(crbug.com/1076571): Move these constants to colors.xml and dimens.xml
    private static final int FAVICON_BACKGROUND_COLOR = 0xff969696;
    // Sets the favicon corner radius to 12.5% of favicon size (2dp for a 16dp favicon)
    private static final float FAVICON_CORNER_RADIUS_FRACTION = 0.125f;
    // Sets the favicon text size to 62.5% of favicon size (10dp for a 16dp favicon)
    private static final float FAVICON_TEXT_SIZE_FRACTION = 0.625f;

    private final Context mContext;
    private ChromeSiteSettingsHelpClient mChromeSiteSettingsHelpClient;
    private ChromeSiteSettingsPrefClient mChromeSiteSettingsPrefClient;
    private ChromeWebappSettingsClient mChromeWebappSettingsClient;
    private ManagedPreferenceDelegate mManagedPreferenceDelegate;

    public ChromeSiteSettingsClient(Context context) {
        mContext = context;
    }

    @Override
    public BrowserContextHandle getBrowserContextHandle() {
        return Profile.getLastUsedRegularProfile();
    }

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
    public SiteSettingsHelpClient getSiteSettingsHelpClient() {
        if (mChromeSiteSettingsHelpClient == null) {
            mChromeSiteSettingsHelpClient = new ChromeSiteSettingsHelpClient();
        }
        return mChromeSiteSettingsHelpClient;
    }

    @Override
    public ChromeSiteSettingsPrefClient getSiteSettingsPrefClient() {
        if (mChromeSiteSettingsPrefClient == null) {
            mChromeSiteSettingsPrefClient = new ChromeSiteSettingsPrefClient();
        }
        return mChromeSiteSettingsPrefClient;
    }

    @Override
    public WebappSettingsClient getWebappSettingsClient() {
        if (mChromeWebappSettingsClient == null) {
            mChromeWebappSettingsClient = new ChromeWebappSettingsClient();
        }
        return mChromeWebappSettingsClient;
    }

    @Override
    public void getFaviconImageForURL(String faviconUrl, Callback<Bitmap> callback) {
        new FaviconLoader(faviconUrl, callback);
    }

    /**
     * A helper class that groups a FaviconHelper with its corresponding Callback.
     *
     * This object is kept alive by being passed to the native
     * FaviconHelper.getLocalFaviconImageForURL. Its reference will be released after the callback
     * has been called.
     */
    private class FaviconLoader implements FaviconImageCallback {
        private final int mFaviconSizePx;
        // Loads the favicons asynchronously.
        private final FaviconHelper mFaviconHelper;
        private final Callback<Bitmap> mCallback;

        private FaviconLoader(String faviconUrl, Callback<Bitmap> callback) {
            mFaviconSizePx =
                    mContext.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
            mCallback = callback;
            mFaviconHelper = new FaviconHelper();

            // TODO(https://crbug.com/1048632): Use the current profile (i.e., regular profile or
            // incognito profile) instead of always using regular profile. It works correctly now,
            // but it is not safe.
            if (!mFaviconHelper.getLocalFaviconImageForURL(
                        Profile.getLastUsedRegularProfile(), faviconUrl, mFaviconSizePx, this)) {
                onFaviconAvailable(/*image=*/null, faviconUrl);
            }
        }

        @Override
        public void onFaviconAvailable(Bitmap image, String iconUrl) {
            mFaviconHelper.destroy();

            if (image == null) {
                // Invalid or no favicon, produce a generic one.
                Resources resources = mContext.getResources();
                float density = resources.getDisplayMetrics().density;
                int faviconSizeDp = Math.round(mFaviconSizePx / density);
                RoundedIconGenerator faviconGenerator =
                        new RoundedIconGenerator(resources, faviconSizeDp, faviconSizeDp,
                                Math.round(FAVICON_CORNER_RADIUS_FRACTION * faviconSizeDp),
                                FAVICON_BACKGROUND_COLOR,
                                Math.round(FAVICON_TEXT_SIZE_FRACTION * faviconSizeDp));
                image = faviconGenerator.generateIconForUrl(iconUrl);
            }
            mCallback.onResult(image);
        }
    }

    @Override
    public boolean isQuietNotificationPromptsFeatureEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.QUIET_NOTIFICATION_PROMPTS);
    }

    @Override
    public String getChannelIdForOrigin(String origin) {
        return SiteChannelsManager.getInstance().getChannelIdForOrigin(origin);
    }
}
