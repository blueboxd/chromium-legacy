// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.provider.Browser;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * A class that handles navigations that should be transformed to intents. Logic taken primarly from
 * //android_webview's AwContentsClient.java:sendBrowsingIntent(), with some additional logic
 * from //android_webview's WebViewBrowserActivity.java:startBrowsingIntent().
 */
@JNINamespace("weblayer")
public class ExternalNavigationHandler {
    private static final String TAG = "ExternalNavHandler";

    static final Pattern BROWSER_URI_SCHEMA =
            Pattern.compile("(?i)" // switch on case insensitive matching
                    + "(" // begin group for schema
                    + "(?:http|https|file)://"
                    + "|(?:inline|data|about|chrome|javascript):"
                    + ")"
                    + ".*");

    @CalledByNative
    private static boolean shouldOverrideUrlLoading(
            String url, boolean hasUserGesture, boolean isRedirect, boolean isMainFrame) {
        // Check for regular URIs that WebLayer supports by itself.
        // TODO(blundell): Port over WebViewBrowserActivity's
        // isSpecializedHandlerAvailable() check that checks whether there's an app for handling
        // the scheme?
        Matcher m = BROWSER_URI_SCHEMA.matcher(url);
        if (m.matches()) {
            return false;
        }

        if (!hasUserGesture && !isRedirect) {
            Log.w(TAG, "Denied starting an intent without a user gesture, URI %s", url);
            return true;
        }

        Intent intent;
        // Perform generic parsing of the URI to turn it into an Intent.
        try {
            intent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
        } catch (Exception ex) {
            Log.w(TAG, "Bad URI %s", url, ex);
            return false;
        }
        // Sanitize the Intent, ensuring web pages can not bypass browser
        // security (only access to BROWSABLE activities).
        intent.addCategory(Intent.CATEGORY_BROWSABLE);

        // Ensure that startActivity() succeeds even if the application context
        // isn't an  Activity. This also matches Chrome's behavior (see
        // //chrome's ExternalNavigationHandler.java:PrepareExternalIntent()).
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        intent.setComponent(null);
        Intent selector = intent.getSelector();
        if (selector != null) {
            selector.addCategory(Intent.CATEGORY_BROWSABLE);
            selector.setComponent(null);
        }

        Context context = ContextUtils.getApplicationContext();

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
