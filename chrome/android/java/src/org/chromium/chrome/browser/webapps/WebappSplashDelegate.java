// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.net.Uri;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.metrics.WebappSplashUmaCache;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserverRegistrar;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.webapk.lib.common.WebApkCommonUtils;
import org.chromium.webapk.lib.common.splash.SplashLayout;

/** Delegate for splash screen for webapps and WebAPKs. */
public class WebappSplashDelegate implements SplashDelegate, NativeInitObserver {
    public static final int HIDE_ANIMATION_DURATION_MS = 300;
    public static final String HISTOGRAM_SPLASHSCREEN_DURATION = "Webapp.Splashscreen.Duration";
    public static final String HISTOGRAM_SPLASHSCREEN_HIDES = "Webapp.Splashscreen.Hides";

    private Activity mActivity;
    private ActivityLifecycleDispatcher mLifecycleDispatcher;
    private TabObserverRegistrar mTabObserverRegistrar;

    /** Whether native was loaded. Native must be loaded in order to record metrics. */
    private boolean mNativeLoaded;

    private WebappInfo mWebappInfo;

    private WebappSplashUmaCache mWebappSplashUmaCache;

    private WebApkSplashNetworkErrorObserver mWebApkNetworkErrorObserver;

    public WebappSplashDelegate(Activity activity, ActivityLifecycleDispatcher lifecycleDispatcher,
            TabObserverRegistrar tabObserverRegistrar, WebappInfo webappInfo) {
        mActivity = activity;
        mLifecycleDispatcher = lifecycleDispatcher;
        mTabObserverRegistrar = tabObserverRegistrar;
        mWebappInfo = webappInfo;

        mLifecycleDispatcher.register(this);

        if (mWebappInfo.isForWebApk()) {
            mWebApkNetworkErrorObserver =
                    new WebApkSplashNetworkErrorObserver(mActivity, mWebappInfo.name());
            mTabObserverRegistrar.registerTabObserver(mWebApkNetworkErrorObserver);
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        mNativeLoaded = true;
        if (mWebappSplashUmaCache != null) mWebappSplashUmaCache.commitMetrics();
    }

    @Override
    public View buildSplashView() {
        Context appContext = ContextUtils.getApplicationContext();
        int backgroundColor =
                ColorUtils.getOpaqueColor(mWebappInfo.backgroundColorFallbackToDefault());
        if (mWebappInfo.isSplashProvidedByWebApk()) {
            return buildSplashWithWebApkProvidedScreenshot(appContext, backgroundColor);
        }
        return buildSplashFromWebApkInfo(appContext, backgroundColor);
    }

    @Override
    public void onSplashHidden(Tab tab, @SplashController.SplashHidesReason int reason,
            long startTimestamp, long endTimestamp) {
        if (mWebApkNetworkErrorObserver != null) {
            mTabObserverRegistrar.unregisterTabObserver(mWebApkNetworkErrorObserver);
            tab.removeObserver(mWebApkNetworkErrorObserver);
            mWebApkNetworkErrorObserver = null;
        }
        mLifecycleDispatcher.unregister(this);

        recordSplashHiddenUma(reason, startTimestamp, endTimestamp);

        mActivity = null;
    }

    @Override
    public boolean shouldWaitForSubsequentPageLoadToHideSplash() {
        return mWebApkNetworkErrorObserver != null
                && mWebApkNetworkErrorObserver.isNetworkErrorDialogVisible();
    }

    /** Builds splash screen from WebApkInfo. */
    private View buildSplashFromWebApkInfo(Context appContext, int backgroundColor) {
        ViewGroup splashScreen = new FrameLayout(appContext);
        splashScreen.setBackgroundColor(backgroundColor);

        if (mWebappInfo.isForWebApk()) {
            initializeWebApkInfoSplashLayout(
                    splashScreen, backgroundColor, ((WebApkInfo) mWebappInfo).splashIcon());
            return splashScreen;
        }

        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorage(mWebappInfo.id());
        if (storage == null) {
            initializeWebApkInfoSplashLayout(splashScreen, backgroundColor, null);
            return splashScreen;
        }

        storage.getSplashScreenImage(new WebappDataStorage.FetchCallback<Bitmap>() {
            @Override
            public void onDataRetrieved(Bitmap splashImage) {
                initializeWebApkInfoSplashLayout(splashScreen, backgroundColor, splashImage);
            }
        });
        return splashScreen;
    }

    private void initializeWebApkInfoSplashLayout(
            ViewGroup splashScreen, int backgroundColor, Bitmap splashImage) {
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();

        Bitmap selectedIcon = splashImage;
        boolean selectedIconGenerated = false;
        // TODO(crbug.com/977173): assign selectedIconAdaptive to correct value
        boolean selectedIconAdaptive = false;
        if (selectedIcon == null) {
            selectedIcon = mWebappInfo.icon();
            selectedIconGenerated = mWebappInfo.isIconGenerated();
            selectedIconAdaptive = mWebappInfo.isIconAdaptive();
        }
        @SplashLayout.IconClassification
        int selectedIconClassification = SplashLayout.classifyIcon(
                context.getResources(), selectedIcon, selectedIconGenerated);

        SplashLayout.createLayout(context, splashScreen, selectedIcon, selectedIconAdaptive,
                selectedIconClassification, mWebappInfo.name(),
                ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor));

        recordWebApkInfoSplashUma(
                resources, selectedIconClassification, selectedIcon, (splashImage != null));
    }

    /**
     * Records UMA metrics for splash screen built from WebApkInfo.
     * @param resources
     * @param selectedIconClassification.
     * @param selectedIcon The icon used on the splash screen.
     * @param usingDedicatedIcon Whether the PWA provides different icons for the splash screen and
     *                           for the app icon.
     */
    private void recordWebApkInfoSplashUma(Resources resources,
            @SplashLayout.IconClassification int selectedIconClassification, Bitmap selectedIcon,
            boolean usingDedicatedIcon) {
        mWebappSplashUmaCache = new WebappSplashUmaCache();
        mWebappSplashUmaCache.recordSplashscreenBackgroundColor(
                mWebappInfo.hasValidBackgroundColor()
                        ? WebappSplashUmaCache.SplashColorStatus.CUSTOM
                        : WebappSplashUmaCache.SplashColorStatus.DEFAULT);
        mWebappSplashUmaCache.recordSplashscreenThemeColor(mWebappInfo.hasValidThemeColor()
                        ? WebappSplashUmaCache.SplashColorStatus.CUSTOM
                        : WebappSplashUmaCache.SplashColorStatus.DEFAULT);

        mWebappSplashUmaCache.recordSplashscreenIconType(
                selectedIconClassification, usingDedicatedIcon);
        if (selectedIconClassification != SplashLayout.IconClassification.INVALID) {
            DisplayMetrics displayMetrics = resources.getDisplayMetrics();
            mWebappSplashUmaCache.recordSplashscreenIconSize(Math.round(
                    selectedIcon.getScaledWidth(displayMetrics) / displayMetrics.density));
        }

        if (mNativeLoaded) mWebappSplashUmaCache.commitMetrics();
    }

    /** Builds splash screen using screenshot provided by WebAPK. */
    private View buildSplashWithWebApkProvidedScreenshot(Context appContext, int backgroundColor) {
        ImageView splashView = new ImageView(appContext);
        splashView.setBackgroundColor(backgroundColor);

        Bitmap splashBitmap = null;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            splashBitmap = FileUtils.queryBitmapFromContentProvider(appContext,
                    Uri.parse(WebApkCommonUtils.generateSplashContentProviderUri(
                            mWebappInfo.webApkPackageName())));
        }
        if (splashBitmap != null) {
            splashView.setScaleType(ImageView.ScaleType.FIT_CENTER);
            splashView.setImageBitmap(splashBitmap);
        }

        return splashView;
    }

    /** Called once the splash screen is hidden to record UMA metrics. */
    private void recordSplashHiddenUma(@SplashController.SplashHidesReason int reason,
            long startTimestamp, long endTimestamp) {
        RecordHistogram.recordEnumeratedHistogram(HISTOGRAM_SPLASHSCREEN_HIDES, reason,
                SplashController.SplashHidesReason.NUM_ENTRIES);

        RecordHistogram.recordMediumTimesHistogram(
                HISTOGRAM_SPLASHSCREEN_DURATION, endTimestamp - startTimestamp);
    }
}
