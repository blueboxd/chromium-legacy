// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;

import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.website.SettingsNavigationSource;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.TrustedWebUtils;

/**
 * Launched by {@link android.support.customtabs.TrustedWebUtils#launchBrowserSiteSettings}.
 * Verifies that url provided in intent has valid Digital Asset Link with the calling application,
 * and if successful, launches site-settings activity for that url.
 */
public class ManageTrustedWebActivityDataActivity extends AppCompatActivity {

    private static final String TAG = "TwaDataActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (LibraryLoader.getInstance().isInitialized()) {
            verifyOriginAndLaunchSettings();
        } else {
            logNativeNotLoaded();
        }
        finish();
    }

    private void verifyOriginAndLaunchSettings() {
        Origin origin = new Origin(getIntent().getData());
        if (isVerifiedOrigin(origin)) {
            new TrustedWebActivityUmaRecorder(ChromeBrowserInitializer.getInstance())
                    .recordOpenedSettingsViaManageSpace();
            startActivity(PreferencesLauncher.createIntentForSingleWebsitePreferences(this,
                    origin.toString(), SettingsNavigationSource.TWA_MANAGE_SPACE_ACTIVITY));
        } else {
            logVerificationFailed();
        }
    }

    private boolean isVerifiedOrigin(Origin origin) {
        CustomTabsSessionToken session =
                CustomTabsSessionToken.getSessionTokenFromIntent(getIntent());
        if (session == null) {
            return false;
        }

        CustomTabsConnection connection =
                ChromeApplication.getComponent().resolveCustomTabsConnection();
        String clientPackageName = connection.getClientPackageNameForSession(session);
        if (clientPackageName == null) {
            return false;
        }

        // We expect that origin has been verified on the client side, and here we synchronously
        // check if a result of a successful verification has been cached.
        return OriginVerifier.wasPreviouslyVerified(clientPackageName, origin,
                CustomTabsService.RELATION_HANDLE_ALL_URLS);
    }

    private void logNativeNotLoaded() {
        Log.e(TAG, "Chrome's native libraries not initialized. Please call CustomTabsClient#warmup"
                + " before TrustedWebUtils#launchBrowserSiteSettings.");
    }

    private void logVerificationFailed() {
        Log.e(TAG, "Failed to verify " + getIntent().getData() + " while launching site settings."
                + " Please use CustomTabsSession#validateRelationship to verify this origin"
                + " before launching an intent with "
                + TrustedWebUtils.ACTION_MANAGE_TRUSTED_WEB_ACTIVITY_DATA);
    }
}
