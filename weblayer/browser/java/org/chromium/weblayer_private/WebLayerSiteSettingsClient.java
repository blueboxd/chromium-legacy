// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.SiteSettingsClient;
import org.chromium.components.browser_ui.site_settings.SiteSettingsHelpClient;
import org.chromium.components.browser_ui.site_settings.SiteSettingsPrefClient;
import org.chromium.components.browser_ui.site_settings.WebappSettingsClient;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Collections;
import java.util.Set;

/**
 * A SiteSettingsClient instance that contains WebLayer-specific Site Settings logic.
 */
public class WebLayerSiteSettingsClient implements SiteSettingsClient {
    private final BrowserContextHandle mBrowserContextHandle;
    private final ManagedPreferenceDelegate mManagedPreferenceDelegate =
            new WebLayerManagedPreferenceDelegate();
    private final SiteSettingsHelpClient mSiteSettingsHelpClient =
            new WebLayerSiteSettingsHelpClient();
    private final SiteSettingsPrefClient mSiteSettingsPrefClient =
            new WebLayerSiteSettingsPrefClient();
    private final WebappSettingsClient mWebappSettingsClient = new WebLayerWebappSettingsClient();

    public WebLayerSiteSettingsClient(BrowserContextHandle browserContextHandle) {
        mBrowserContextHandle = browserContextHandle;
    }

    @Override
    public BrowserContextHandle getBrowserContextHandle() {
        return mBrowserContextHandle;
    }

    @Override
    public ManagedPreferenceDelegate getManagedPreferenceDelegate() {
        return mManagedPreferenceDelegate;
    }

    @Override
    public SiteSettingsHelpClient getSiteSettingsHelpClient() {
        return mSiteSettingsHelpClient;
    }

    @Override
    public SiteSettingsPrefClient getSiteSettingsPrefClient() {
        return mSiteSettingsPrefClient;
    }

    @Override
    public WebappSettingsClient getWebappSettingsClient() {
        return mWebappSettingsClient;
    }

    @Override
    public void getFaviconImageForURL(String faviconUrl, Callback<Bitmap> callback) {
        // We don't currently support favicons on WebLayer.
        callback.onResult(null);
    }

    @Override
    public boolean isCategoryVisible(@SiteSettingsCategory.Type int type) {
        // TODO: hide unsupported categories
        return true;
    }

    @Override
    public boolean isQuietNotificationPromptsFeatureEnabled() {
        return false;
    }

    @Override
    public String getChannelIdForOrigin(String origin) {
        return null;
    }

    /**
     * No-op ManagedPreferenceDelegate implementation since WebLayer doesn't support managed
     * preferences.
     */
    private static class WebLayerManagedPreferenceDelegate implements ManagedPreferenceDelegate {
        @Override
        public boolean isPreferenceControlledByPolicy(Preference preference) {
            return false;
        }

        @Override
        public boolean isPreferenceControlledByCustodian(Preference preference) {
            return false;
        }

        @Override
        public boolean doesProfileHaveMultipleCustodians() {
            return false;
        }
    }

    /**
     * No-op SiteSettingsHelpClient implementation since WebLayer doesn't have help pages.
     */
    private static class WebLayerSiteSettingsHelpClient implements SiteSettingsHelpClient {
        @Override
        public boolean isHelpAndFeedbackEnabled() {
            return false;
        }

        @Override
        public void launchSettingsHelpAndFeedbackActivity(Activity currentActivity) {}

        @Override
        public void launchProtectedContentHelpAndFeedbackActivity(Activity currentActivity) {}
    }

    // TODO: Wire up a JNI interface for this.
    private static class WebLayerSiteSettingsPrefClient implements SiteSettingsPrefClient {
        @Override
        public boolean getBlockThirdPartyCookies() {
            return false;
        }
        @Override
        public void setBlockThirdPartyCookies(boolean newValue) {}
        @Override
        public boolean isBlockThirdPartyCookiesManaged() {
            return false;
        }

        @Override
        public int getCookieControlsMode() {
            return 0;
        }
        @Override
        public void setCookieControlsMode(int newValue) {}

        @Override
        public boolean getEnableQuietNotificationPermissionUi() {
            return false;
        }
        @Override
        public void setEnableQuietNotificationPermissionUi(boolean newValue) {}
        @Override
        public void clearEnableNotificationPermissionUi() {}

        @Override
        public void setNotificationsVibrateEnabled(boolean newValue) {}
    }

    /**
     * No-op WebappSettingsClient implementation since WebLayer doesn't support webapps.
     */
    private static class WebLayerWebappSettingsClient implements WebappSettingsClient {
        @Override
        public Set<String> getOriginsWithInstalledApp() {
            return Collections.EMPTY_SET;
        }

        @Override
        public Set<String> getAllDelegatedNotificationOrigins() {
            return Collections.EMPTY_SET;
        }

        @Override
        @Nullable
        public String getNotificationDelegateAppNameForOrigin(Origin origin) {
            return null;
        }

        @Override
        @Nullable
        public String getNotificationDelegatePackageNameForOrigin(Origin origin) {
            return null;
        }
    }
}
