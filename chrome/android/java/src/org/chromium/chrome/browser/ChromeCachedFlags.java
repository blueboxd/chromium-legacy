// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.FeatureUtilities;

import java.util.Arrays;
import java.util.List;

/**
 * Caches the flags that Chrome might require before native is loaded in a later next run.
 */
public class ChromeCachedFlags {
    private boolean mIsFinishedCachingNativeFlags;

    private static final ChromeCachedFlags INSTANCE = new ChromeCachedFlags();

    /**
     * @return The {@link ChromeCachedFlags} singleton.
     */
    public static ChromeCachedFlags getInstance() {
        return INSTANCE;
    }

    /**
     * Caches flags that are needed by Activities that launch before the native library is loaded
     * and stores them in SharedPreferences. Because this function is called during launch after the
     * library has loaded, they won't affect the next launch until Chrome is restarted.
     */
    public void cacheNativeFlags() {
        if (mIsFinishedCachingNativeFlags) return;
        FirstRunUtils.cacheFirstRunPrefs();

        List<String> featuresToCache = Arrays.asList(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY,
                ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED, ChromeFeatureList.CHROME_DUET,
                ChromeFeatureList.CHROME_DUET_ADAPTIVE, ChromeFeatureList.CHROME_DUET_LABELED,
                ChromeFeatureList.ANDROID_NIGHT_MODE_CCT,
                ChromeFeatureList.DOWNLOADS_AUTO_RESUMPTION_NATIVE,
                ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS,
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
                ChromeFeatureList.IMMERSIVE_UI_MODE,
                ChromeFeatureList.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT,
                ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.PAINT_PREVIEW_TEST);
        FeatureUtilities.cacheNativeFlags(featuresToCache);
        FeatureUtilities.cacheAdditionalNativeFlags();
        mIsFinishedCachingNativeFlags = true;
    }

    /**
     * Caches flags that are enabled in ServiceManager only mode and must take effect on startup but
     * are set via native code. This function needs to be called in ServiceManager only mode to mark
     * these field trials as active, otherwise histogram data recorded in ServiceManager only mode
     * won't be tagged with their corresponding field trial experiments.
     */
    public void cacheServiceManagerOnlyFlags() {
        // TODO(crbug.com/995355): Move other related flags from cacheNativeFlags() to here.
        FeatureUtilities.cacheNativeFlags(
                Arrays.asList(ChromeFeatureList.SERVICE_MANAGER_FOR_DOWNLOAD,
                        ChromeFeatureList.SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH));
    }
}
