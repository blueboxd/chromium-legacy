// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

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
            Flag.commandLine("show-composited-layer-borders",
                    "Renders a border around compositor layers to help debug and study layer "
                            + "compositing."),
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
            Flag.baseFeature("VizForWebView", "Enables Viz for WebView."),
            Flag.baseFeature("WebViewConnectionlessSafeBrowsing",
                    "Uses GooglePlayService's 'connectionless' APIs for Safe Browsing "
                            + "security checks."),
            Flag.baseFeature(
                    "WebViewBrotliSupport", "Enables brotli compression support in WebView."),
            Flag.baseFeature("SafeBrowsingCommittedInterstitials",
                    "Commits Safe Browsing warning pages like page navigations."),
    };
}
