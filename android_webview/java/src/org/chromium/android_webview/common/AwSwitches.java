// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

/**
 * Contains command line switches that are specific to Android WebView.
 */
public final class AwSwitches {
    // Native switch kEnableCrashReporterForTesting in //base/base_switches.h
    public static final String CRASH_UPLOADS_ENABLED_FOR_TESTING_SWITCH =
            "enable-crash-reporter-for-testing";

    // Highlight the contents (including web contents) of all WebViews with a yellow tint. This is
    // useful for identifying WebViews in an Android application.
    public static final String HIGHLIGHT_ALL_WEBVIEWS = "highlight-all-webviews";

    // WebView will log additional debugging information to logcat, such as variations and
    // commandline state.
    public static final String WEBVIEW_VERBOSE_LOGGING = "webview-verbose-logging";

    // Allow mirroring JS console messages to system logs. This is the default behavior on
    // debuggable devices (userdebug or eng), so there is no reason for a user to specify this
    // explicitly. Native switch kWebViewLogJsConsoleMessages.
    public static final String WEBVIEW_LOG_JS_CONSOLE_MESSAGES = "webview-log-js-console-messages";

    // Indicate that renderers are running in a sandbox. Enables
    // kInProcessGPU and sets kRendererProcessLimit to 1.
    // Native switch kWebViewSandboxedRenderer.
    public static final String WEBVIEW_SANDBOXED_RENDERER = "webview-sandboxed-renderer";

    // Disables safebrowsing functionality in webview.
    // Native switch kWebViewDisableSafeBrowsingSupport.
    public static final String WEBVIEW_DISABLE_SAFEBROWSING_SUPPORT =
            "webview-disable-safebrowsing-support";

    // Enables SafeBrowsing and causes WebView to treat all resources as malicious. Use care: this
    // will block all resources from loading.
    // No native switch.
    public static final String WEBVIEW_SAFEBROWSING_BLOCK_ALL_RESOURCES =
            "webview-safebrowsing-block-all-resources";

    // The length of time in seconds that an app's copy of the variations seed should be considered
    // fresh. If an app's seed is older than this, a new seed will be requested from WebView's
    // IVariationsSeedServer.
    // No native switch.
    public static final String FINCH_SEED_EXPIRATION_AGE = "finch-seed-expiration-age";

    // Forces WebView's service to always schedule a new variations seed download job, even if one
    // is already pending.
    // No native switch.
    public static final String FINCH_SEED_IGNORE_PENDING_DOWNLOAD =
            "finch-seed-ignore-pending-download";

    // The minimum amount of time in seconds that WebView's service will wait between two
    // variations seed downloads from the variations server.
    // No native switch.
    public static final String FINCH_SEED_MIN_DOWNLOAD_PERIOD = "finch-seed-min-download-period";

    // The minimum amount of time in seconds that the embedded WebView implementation will wait
    // between two requests to WebView's service for a new variations seed.
    // No native switch.
    public static final String FINCH_SEED_MIN_UPDATE_PERIOD = "finch-seed-min-update-period";

    // Do not instantiate this class.
    private AwSwitches() {}
}
