// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import org.chromium.components.metrics.MetricsSwitches;

/**
 * List of experimental features/flags supported for user devices. Add features/flags to this list
 * with scrutiny: any feature/flag in this list can be enabled for production Android devices, and
 * so it must not compromise the Android security model (i.e., WebView must still protect the app's
 * private data from being user visible).
 *
 * <p>
 * This lives in the common package so it can be shared by dev UI (to know which features/flags to
 * display) as well as the WebView implementation (so it knows which features/flags are safe to
 * honor).
 */
public final class ProductionSupportedFlagList {
    // Do not instantiate this class.
    private ProductionSupportedFlagList() {}

    /**
     * A list of commandline flags supported on user devices.
     */
    public static final Flag[] sFlagList = {
            Flag.commandLine(AwSwitches.HIGHLIGHT_ALL_WEBVIEWS,
                    "Highlight the contents (including web contents) of all WebViews with a yellow "
                            + "tint. This is useful for identifying WebViews in an Android "
                            + "application."),
            Flag.commandLine(AwSwitches.WEBVIEW_VERBOSE_LOGGING,
                    "WebView will log additional debugging information to logcat, such as "
                            + "variations and commandline state."),
            Flag.commandLine("show-composited-layer-borders",
                    "Renders a border around compositor layers to help debug and study layer "
                            + "compositing."),
            Flag.commandLine(AwSwitches.FINCH_SEED_EXPIRATION_AGE,
                    "Forces all variations seeds to be considered stale.", "0"),
            Flag.commandLine(AwSwitches.FINCH_SEED_IGNORE_PENDING_DOWNLOAD,
                    "Forces the WebView service to reschedule a variations seed download job even "
                            + "if one is already pending."),
            Flag.commandLine(AwSwitches.FINCH_SEED_MIN_DOWNLOAD_PERIOD,
                    "Disables throttling of variations seed download jobs.", "0"),
            Flag.commandLine(AwSwitches.FINCH_SEED_MIN_UPDATE_PERIOD,
                    "Disables throttling of new variations seed requests to the WebView service.",
                    "0"),
            Flag.commandLine(MetricsSwitches.FORCE_ENABLE_METRICS_REPORTING,
                    "Forces WebView's metrics reporting to be enabled. This overrides user "
                            + "settings and capacity sampling, but does not override the app's "
                            + "choice to opt-out."),
            Flag.commandLine("webview-log-js-console-messages",
                    "Mirrors JavaScript console messages to system logs."),
            Flag.commandLine(AwSwitches.CRASH_UPLOADS_ENABLED_FOR_TESTING_SWITCH,
                    "Used for turning on Breakpad crash reporting in a debug environment where "
                            + "crash reporting is typically compiled but disabled."),
            Flag.commandLine("disable-gpu-rasterization",
                    "Disables GPU rasterization, i.e. rasterizes on the CPU only."),
            Flag.baseFeature("OutOfBlinkCors",
                    "Moves CORS logic into the Network Service (rather than inside the blink "
                            + "rendering engine)."),
            Flag.baseFeature("EnableSharedImageForWebview", "Enables shared images for WebView."),
            Flag.baseFeature("VizForWebView", "Enables Viz for WebView."),
            Flag.baseFeature("WebViewConnectionlessSafeBrowsing",
                    "Uses GooglePlayService's 'connectionless' APIs for Safe Browsing "
                            + "security checks."),
            Flag.baseFeature(
                    "WebViewBrotliSupport", "Enables brotli compression support in WebView."),
            Flag.baseFeature("SafeBrowsingCommittedInterstitials",
                    "Commits Safe Browsing warning pages like page navigations."),
            Flag.baseFeature(
                    "AppCache", "Controls AppCache to facilitate testing against future removal."),
    };
}
