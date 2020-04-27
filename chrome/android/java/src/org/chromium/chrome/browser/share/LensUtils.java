// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;

/**
 * This class provides utilities for intenting into Google Lens.
 */
public class LensUtils {
    private static final String LENS_CONTRACT_URI = "googleapp://lens";
    private static final String LENS_BITMAP_URI_KEY = "LensBitmapUriKey";
    private static final String ACCOUNT_NAME_URI_KEY = "AccountNameUriKey";
    private static final String INCOGNITO_URI_KEY = "IncognitoUriKey";
    private static final String LAUNCH_TIMESTAMP_URI_KEY = "ActivityLaunchTimestampNanos";
    private static final String MIN_AGSA_VERSION_FEATURE_PARAM_NAME = "minAgsaVersionName";
    private static final String USE_SEARCH_BY_IMAGE_TEXT_FEATURE_PARAM_NAME =
            "useSearchByImageText";
    private static final String LOG_UKM_PARAM_NAME = "logUkm";
    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE = "8.19";

    /**
     * See function for details.
     */
    private static boolean sFakePassableLensEnvironmentForTesting;

    /*
     * If true, short-circuit the version name intent check to always return a high enough version.
     * Also hardcode the device OS check to return true.
     * Used by test cases.
     * @param shouldFake Whether to fake the version check.
     */
    public static void setFakePassableLensEnvironmentForTesting(boolean shouldFake) {
        sFakePassableLensEnvironmentForTesting = shouldFake;
    }

    /**
     * Resolve the activity to verify that lens is ready to accept an intent and also
     * retrieve the version name.
     *
     * @param context The relevant application context with access to the activity.
     * @return The version name string of the AGSA app or an empty string if not available.
     */
    public static String getLensActivityVersionNameIfAvailable(Context context) {
        // Use this syntax to avoid NPE if unset.
        if (Boolean.TRUE.equals(sFakePassableLensEnvironmentForTesting)) {
            // Returns the minimum version which will meet the bar and allow future AGSA version
            // checks to succeed.
            return MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE;
        } else {
            try {
                PackageManager pm = context.getPackageManager();
                // No data transmission occurring so safe to assume incognito is false.
                Intent lensIntent =
                        getShareWithGoogleLensIntent(Uri.EMPTY, /* isIncognito= */ false,
                                /* currentTimeNanos= */ 0L);
                ComponentName lensActivity = lensIntent.resolveActivity(pm);
                if (lensActivity == null) return "";
                PackageInfo packageInfo = pm.getPackageInfo(lensActivity.getPackageName(), 0);
                if (packageInfo == null) {
                    return "";
                } else {
                    return packageInfo.versionName;
                }
            } catch (PackageManager.NameNotFoundException e) {
                return "";
            }
        }
    }

    /**
     * Gets the minimum AGSA version required to support the Lens context menu integration
     * on this device.  Takes the value from a server provided value if a field trial is
     * active but otherwise will take the value from a client side default (unless the
     * lens feature is not enabled at all, in which case return an empty string).
     *
     * @return The minimum version name string or an empty string if not available.
     */
    public static String getMinimumAgsaVersionForLensSupport() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS)) {
            final String serverProvidedMinAgsaVersion =
                    ChromeFeatureList.getFieldTrialParamByFeature(
                            ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                            MIN_AGSA_VERSION_FEATURE_PARAM_NAME);
            if (TextUtils.isEmpty(serverProvidedMinAgsaVersion)) {
                // Falls into this block if the user enabled the feature using chrome://flags and
                // the param was not set by the server.
                return MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE;
            }
            return serverProvidedMinAgsaVersion;
        }
        // The feature is disabled so no need to return a minimum version.
        return "";
    }

    /**
     * Checks whether the device is below Android O.  We restrict to these versions
     * to limit to OS"s where image processing vulnerabilities can be retroactively
     * fixed if they are discovered in the future.
     * @return Whether the device is below Android O.
     */
    public static boolean isDeviceOsBelowMinimum() {
        if (sFakePassableLensEnvironmentForTesting) {
            return false;
        }

        return Build.VERSION.SDK_INT < Build.VERSION_CODES.O;
    }

    /**
     * Checks whether the GSA package on the device is guaranteed to be an official GSA build.
     * @return Whether the package is valid.
     */
    public static boolean isValidAgsaPackage(ExternalAuthUtils externalAuthUtils) {
        if (sFakePassableLensEnvironmentForTesting) {
            return true;
        }

        return externalAuthUtils.isGoogleSigned(IntentHandler.PACKAGE_GSA);
    }

    /**
     * Get a deeplink intent to Google Lens with an optional content provider image URI. The intent
     * should be constructed immediately before the intent is fired to ensure that the launch
     * timestamp is accurate.
     * @param imageUri The content provider URI generated by chrome (or empty URI)
     *                 if only resolving the activity.
     * @param isIncognito Whether the current tab is in incognito mode.
     * @param currentTimeNanos The current system time since boot in nanos.
     * @return The intent to Google Lens.
     */

    public static Intent getShareWithGoogleLensIntent(
            Uri imageUri, boolean isIncognito, long currentTimeNanos) {
        CoreAccountInfo coreAccountInfo =
                IdentityServicesProvider.get().getIdentityManager().getPrimaryAccountInfo(
                        ConsentLevel.SYNC);
        // If incognito do not send the account name to avoid leaking session information to Lens.
        String signedInAccountName =
                (coreAccountInfo == null || isIncognito) ? "" : coreAccountInfo.getEmail();

        Uri lensUri = Uri.parse(LENS_CONTRACT_URI);
        if (!Uri.EMPTY.equals(imageUri)) {
            lensUri =
                    lensUri.buildUpon()
                            .appendQueryParameter(LENS_BITMAP_URI_KEY, imageUri.toString())
                            .appendQueryParameter(ACCOUNT_NAME_URI_KEY, signedInAccountName)
                            .appendQueryParameter(INCOGNITO_URI_KEY, Boolean.toString(isIncognito))
                            .appendQueryParameter(
                                    LAUNCH_TIMESTAMP_URI_KEY, Long.toString(currentTimeNanos))
                            .build();
            ContextUtils.getApplicationContext().grantUriPermission(
                    IntentHandler.PACKAGE_GSA, imageUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }

        Intent intent = new Intent(Intent.ACTION_VIEW).setData(lensUri);
        intent.setPackage(IntentHandler.PACKAGE_GSA);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        return intent;
    }

    public static boolean enableGoogleLensFeature() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS);
    }

    /**
     * Whether to display the lens menu item with the search by image text
     */
    public static boolean useLensWithSearchByImageText() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                USE_SEARCH_BY_IMAGE_TEXT_FEATURE_PARAM_NAME, false);
    }

    /*
     * Whether to log UKM pings for lens-related behavior.
     * If in the experiment will log by default and will only be disabled
     * if the parameter is not absent and set to true.
     */
    public static boolean shouldLogUkm() {
        if (enableGoogleLensFeature()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS, LOG_UKM_PARAM_NAME,
                    true);
        }

        return false;
    }
}
