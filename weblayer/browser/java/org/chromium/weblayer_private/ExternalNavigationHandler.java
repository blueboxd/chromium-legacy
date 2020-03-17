// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.provider.Browser;

import org.chromium.base.Log;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.PageTransition;

/**
 * A class that handles navigations that should be transformed to intents. Logic taken primarly from
 * //android_webview's AwContentsClient.java:sendBrowsingIntent(), with some additional logic
 * from //android_webview's WebViewBrowserActivity.java:startBrowsingIntent() and
 * //components/external_intents' ExternalNavigationHandler.java.
 * TODO(crbug.com/1031465): Eliminate this custom class entirely in favor of using
 * the //component-level ExternalNavigationHandler.java that //chrome uses.
 */
public class ExternalNavigationHandler {
    private static final String TAG = "ExternalNavHandler";

    /**
     * The "about:", "chrome:", "chrome-native:", and "devtools:" schemes
     * are internal to the browser; don't want these to be dispatched to other apps.
     */
    private static boolean hasInternalScheme(
            String url, Intent targetIntent, boolean hasIntentScheme) {
        if (hasIntentScheme) {
            // TODO(https://crbug.com/783819): When this function is converted to GURL, we should
            // also call fixUpUrl on this user-provided URL as the fixed-up URL is what we would end
            // up navigating to.
            url = targetIntent.getDataString();
        }
        if (url.startsWith(ContentUrlConstants.ABOUT_SCHEME)
                || url.startsWith(UrlConstants.CHROME_URL_SHORT_PREFIX)
                || url.startsWith(UrlConstants.CHROME_NATIVE_URL_SHORT_PREFIX)
                || url.startsWith(UrlConstants.DEVTOOLS_URL_SHORT_PREFIX)) {
            return true;
        }
        return false;
    }

    /** The "content:" scheme is disabled in WebLayer. Do not try to start an activity. */
    private static boolean hasContentScheme(
            String url, Intent targetIntent, boolean hasIntentScheme) {
        if (hasIntentScheme) {
            url = targetIntent.getDataString();
        }
        if (!url.startsWith(UrlConstants.CONTENT_URL_SHORT_PREFIX)) return false;
        return true;
    }

    static boolean shouldOverrideUrlLoading(TabImpl tab, String url, boolean hasUserGesture,
            boolean isRedirect, boolean isMainFrame, int pageTransition) {
        if (UrlUtilities.isAcceptedScheme(url)) return false;

        // A back-forward navigation should never trigger an intent.
        if ((pageTransition & PageTransition.FORWARD_BACK) != 0) return false;

        Intent intent;
        // Perform generic parsing of the URI to turn it into an Intent.
        try {
            intent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
        } catch (Exception ex) {
            Log.w(TAG, "Bad URI %s", url, ex);
            return false;
        }

        boolean hasIntentScheme = url.startsWith(UrlConstants.INTENT_URL_SHORT_PREFIX)
                || url.startsWith(UrlConstants.APP_INTENT_URL_SHORT_PREFIX);
        if (hasInternalScheme(url, intent, hasIntentScheme)) {
            return false;
        }

        if (hasContentScheme(url, intent, hasIntentScheme)) {
            return false;
        }

        if (!hasUserGesture && !isRedirect) {
            Log.w(TAG, "Denied starting an intent without a user gesture, URI %s", url);
            return true;
        }

        // Sanitize the Intent, ensuring web pages can not bypass browser
        // security (only access to BROWSABLE activities).
        intent.addCategory(Intent.CATEGORY_BROWSABLE);

        // Match Chrome's behavior (see //chrome's
        // ExternalNavigationHandler.java:PrepareExternalIntent()).
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        intent.setComponent(null);
        Intent selector = intent.getSelector();
        if (selector != null) {
            selector.addCategory(Intent.CATEGORY_BROWSABLE);
            selector.setComponent(null);
        }

        Context context = tab.getBrowser().getContext();

        if (context == null) {
            return false;
        }

        // Pass the package name as application ID so that the intent from the
        // same application can be opened in the same tab.
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());

        try {
            context.startActivity(intent);
            return true;
        } catch (ActivityNotFoundException ex) {
            Log.w(TAG, "No application can handle %s", url);
        } catch (SecurityException ex) {
            // This can happen if the Activity is exported="true", guarded by a permission, and sets
            // up an intent filter matching this intent. This is a valid configuration for an
            // Activity, so instead of crashing, we catch the exception and do nothing. See
            // https://crbug.com/808494 and https://crbug.com/889300.
            Log.w(TAG, "SecurityException when starting intent for %s", url);
        }

        return false;
    }
}
