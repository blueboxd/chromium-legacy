// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory.Type;
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
@JNINamespace("weblayer")
public class WebLayerSiteSettingsClient
        implements SiteSettingsClient, ManagedPreferenceDelegate, SiteSettingsHelpClient,
                   SiteSettingsPrefClient, WebappSettingsClient {
    private final BrowserContextHandle mBrowserContextHandle;

    public WebLayerSiteSettingsClient(BrowserContextHandle browserContextHandle) {
        mBrowserContextHandle = browserContextHandle;
    }

    // SiteSettingsClient implementation:

    @Override
    public BrowserContextHandle getBrowserContextHandle() {
        return mBrowserContextHandle;
    }

    @Override
    public ManagedPreferenceDelegate getManagedPreferenceDelegate() {
        return this;
    }

    @Override
    public SiteSettingsHelpClient getSiteSettingsHelpClient() {
        return this;
    }

    @Override
    public SiteSettingsPrefClient getSiteSettingsPrefClient() {
        return this;
    }

    @Override
    public WebappSettingsClient getWebappSettingsClient() {
        return this;
    }

    @Override
    public void getFaviconImageForURL(String faviconUrl, Callback<Bitmap> callback) {
        // We don't currently support favicons on WebLayer.
        callback.onResult(null);
    }

    @Override
    public boolean isCategoryVisible(@Type int type) {
        return type == Type.ALL_SITES || type == Type.AUTOMATIC_DOWNLOADS || type == Type.CAMERA
                || type == Type.COOKIES || type == Type.DEVICE_LOCATION || type == Type.JAVASCRIPT
                || type == Type.MICROPHONE || type == Type.PROTECTED_MEDIA || type == Type.SOUND
                || type == Type.USE_STORAGE;
    }

    @Override
    public boolean isQuietNotificationPromptsFeatureEnabled() {
        return false;
    }

    @Override
    public String getChannelIdForOrigin(String origin) {
        return null;
    }

    // ManagedPrefrenceDelegate implementation:
    // A no-op because WebLayer doesn't support managed preferences.

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

    // SiteSettingsHelpClient implementation:
    // A no-op since WebLayer doesn't have help pages.

    @Override
    public boolean isHelpAndFeedbackEnabled() {
        return false;
    }

    @Override
    public void launchSettingsHelpAndFeedbackActivity(Activity currentActivity) {}

    @Override
    public void launchProtectedContentHelpAndFeedbackActivity(Activity currentActivity) {}

    // SiteSettingsPrefClient implementation:
    // TODO(crbug.com/1071603): Once PrefServiceBridge is componentized we can get rid of the JNI
    //                          methods here and call PrefServiceBridge directly.

    @Override
    public boolean getBlockThirdPartyCookies() {
        return WebLayerSiteSettingsClientJni.get().getBlockThirdPartyCookies(mBrowserContextHandle);
    }
    @Override
    public void setBlockThirdPartyCookies(boolean newValue) {
        WebLayerSiteSettingsClientJni.get().setBlockThirdPartyCookies(
                mBrowserContextHandle, newValue);
    }
    @Override
    public boolean isBlockThirdPartyCookiesManaged() {
        // WebLayer doesn't support managed prefs.
        return false;
    }

    @Override
    public int getCookieControlsMode() {
        return WebLayerSiteSettingsClientJni.get().getCookieControlsMode(mBrowserContextHandle);
    }
    @Override
    public void setCookieControlsMode(int newValue) {
        WebLayerSiteSettingsClientJni.get().setCookieControlsMode(mBrowserContextHandle, newValue);
    }

    // The quiet notification UI is a Chrome-specific feature for now.
    @Override
    public boolean getEnableQuietNotificationPermissionUi() {
        return false;
    }
    @Override
    public void setEnableQuietNotificationPermissionUi(boolean newValue) {}
    @Override
    public void clearEnableNotificationPermissionUi() {}

    // WebLayer doesn't support notifications yet.
    @Override
    public void setNotificationsVibrateEnabled(boolean newValue) {}

    // WebappSettingsClient implementation:
    // A no-op since WebLayer doesn't support webapps.

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

    @NativeMethods
    interface Natives {
        boolean getBlockThirdPartyCookies(BrowserContextHandle browserContextHandle);
        void setBlockThirdPartyCookies(BrowserContextHandle browserContextHandle, boolean newValue);

        int getCookieControlsMode(BrowserContextHandle browserContextHandle);
        void setCookieControlsMode(BrowserContextHandle browserContextHandle, int newValue);
    }
}
