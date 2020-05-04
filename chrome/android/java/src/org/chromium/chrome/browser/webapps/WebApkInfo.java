// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.trusted.sharing.ShareData;

import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.webapps.WebApkExtras.ShortcutItem;

import java.util.List;
import java.util.Map;

/**
 * Stores info for WebAPK.
 */
public class WebApkInfo extends WebappInfo {
    /**
     * Constructs a WebApkInfo from the passed in Intent and <meta-data> in the WebAPK's Android
     * manifest.
     * @param intent Intent containing info about the app.
     */
    public static WebappInfo create(Intent intent) {
        return create(WebApkIntentDataProviderFactory.create(intent));
    }

    /**
     * Constructs a WebApkInfo from the passed in parameters and <meta-data> in the WebAPK's Android
     * manifest.
     *
     * @param webApkPackageName The package name of the WebAPK.
     * @param url Url that the WebAPK should navigate to when launched.
     * @param source Source that the WebAPK was launched from.
     * @param forceNavigation Whether the WebAPK should navigate to {@link url} if it is already
     *                        running.
     * @param canUseSplashFromContentProvider Whether the WebAPK's content provider can be
     *                                        queried for a screenshot of the splash screen.
     * @param shareData Shared information from the share intent.
     * @param shareDataActivityClassName Name of WebAPK activity which received share intent.
     */
    public static WebappInfo create(String webApkPackageName, String url, int source,
            boolean forceNavigation, boolean canUseSplashFromContentProvider, ShareData shareData,
            String shareDataActivityClassName) {
        return create(WebApkIntentDataProviderFactory.create(webApkPackageName, url, source,
                forceNavigation, canUseSplashFromContentProvider, shareData,
                shareDataActivityClassName));
    }

    /**
     * Construct a {@link WebApkInfo} instance.
     * @param url                      URL that the WebAPK should navigate to when launched.
     * @param scope                    Scope for the WebAPK.
     * @param primaryIcon              Primary icon to show for the WebAPK.
     * @param splashIcon               Splash icon to use for the splash screen.
     * @param name                     Name of the WebAPK.
     * @param shortName                The short name of the WebAPK.
     * @param displayMode              Display mode of the WebAPK.
     * @param orientation              Orientation of the WebAPK.
     * @param source                   Source that the WebAPK was launched from.
     * @param themeColor               The theme color of the WebAPK.
     * @param backgroundColor          The background color of the WebAPK.
     * @param defaultBackgroundColor   The background color to use if the Web Manifest does not
     *                                 provide a background color.
     * @param isPrimaryIconMaskable    Is the primary icon maskable.
     * @param webApkPackageName        The package of the WebAPK.
     * @param shellApkVersion          Version of the code in //chrome/android/webapk/shell_apk.
     * @param manifestUrl              URL of the Web Manifest.
     * @param manifestStartUrl         URL that the WebAPK should navigate to when launched from
     *                                 the homescreen. Different from the {@link url} parameter if
     *                                 the WebAPK is launched from a deep link.
     * @param distributor              The source from where the WebAPK is installed.
     * @param iconUrlToMurmur2HashMap  Map of the WebAPK's icon URLs to Murmur2 hashes of the
     *                                 icon untransformed bytes.
     * @param shareTarget              Specifies what share data is supported by WebAPK.
     * @param forceNavigation          Whether the WebAPK should navigate to {@link url} if the
     *                                 WebAPK is already open.
     * @param isSplashProvidedByWebApk Whether the WebAPK (1) launches an internal activity to
     *                                 display the splash screen and (2) has a content provider
     *                                 which provides a screenshot of the splash screen.
     * @param shareData                Shared information from the share intent.
     * @param webApkVersionCode        WebAPK's version code.
     */
    public static WebappInfo create(String url, String scope, WebappIcon primaryIcon,
            WebappIcon splashIcon, String name, String shortName, @WebDisplayMode int displayMode,
            int orientation, int source, long themeColor, long backgroundColor,
            int defaultBackgroundColor, boolean isPrimaryIconMaskable, boolean isSplashIconMaskable,
            String webApkPackageName, int shellApkVersion, String manifestUrl,
            String manifestStartUrl, @WebApkDistributor int distributor,
            Map<String, String> iconUrlToMurmur2HashMap, WebApkShareTarget shareTarget,
            boolean forceNavigation, boolean isSplashProvidedByWebApk, ShareData shareData,
            List<ShortcutItem> shortcutItems, int webApkVersionCode) {
        return create(WebApkIntentDataProviderFactory.create(url, scope, primaryIcon, splashIcon,
                name, shortName, displayMode, orientation, source, themeColor, backgroundColor,
                defaultBackgroundColor, isPrimaryIconMaskable, isSplashIconMaskable,
                webApkPackageName, shellApkVersion, manifestUrl, manifestStartUrl, distributor,
                iconUrlToMurmur2HashMap, shareTarget, forceNavigation, isSplashProvidedByWebApk,
                shareData, shortcutItems, webApkVersionCode));
    }

    private static WebappInfo create(@Nullable BrowserServicesIntentDataProvider provider) {
        return (provider != null) ? new WebApkInfo(provider) : null;
    }

    public WebApkInfo(@NonNull BrowserServicesIntentDataProvider provider) {
        super(provider);
    }
}
