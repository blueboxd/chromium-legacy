// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;

import com.android.webview.chromium.MonochromeLibraryPreloader;

import org.chromium.android_webview.nonembedded.WebViewApkApplication;
import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.content_public.browser.ChildProcessCreationParams;

/**
 * This is Application class for Monochrome.
 *
 * You shouldn't add anything else in this file, this class is split off from
 * normal chrome in order to access Android system API through Android WebView
 * glue layer and have monochrome specific code.
 *
 * This class is NOT used by Trichrome. Do not add anything here which is only
 * related to Monochrome's minimum SDK level or APK packaging decisions, because
 * those are likely to apply to Trichrome as well - this must only be used for
 * things specific to functioning as a WebView implementation.
 */
public class MonochromeApplication extends ChromeApplication {
    @Override
    protected void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        WebViewApkApplication.maybeInitProcessGlobals();
        if (!LibraryLoader.getInstance().isLoadedByZygote()) {
            LibraryLoader.getInstance().setNativeLibraryPreloader(new MonochromeLibraryPreloader());
        }
        // ChildProcessCreationParams is only needed for browser process, though it is
        // created and set in all processes. We must set isExternalService to true for
        // Monochrome because Monochrome's renderer services are shared with WebView
        // and are external, and will fail to bind otherwise.
        boolean bindToCaller = false;
        boolean ignoreVisibilityForImportance = false;
        ChildProcessCreationParams.set(getPackageName(), null /* privilegedServicesName */,
                getPackageName(), null /* sandboxedServicesName */, true /* isExternalService */,
                LibraryProcessType.PROCESS_CHILD, bindToCaller, ignoreVisibilityForImportance);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (!ChromeVersionInfo.isStableBuild()) {
            // Performing Monochrome WebView DevTools Launcher icon showing/hiding logic in onCreate
            // rather than in attachBaseContext() because it depends on application context being
            // initiatied.
            if (isWebViewProcess()) {
                // Whenever a monochrome webview process is launched (WebView service or developer
                // UI), post a background task to show/hide the DevTools icon.
                WebViewApkApplication.postDeveloperUiLauncherIconTask();
            } else if (isBrowserProcess()) {
                // Frequently check current system webview provider and show/hide the icon
                // accordingly by listening to Monochrome browser Activities status (whenever a
                // browser activity comes to the foreground).
                ApplicationStatus.registerStateListenerForAllActivities((activity, state) -> {
                    if (state == ActivityState.STARTED) {
                        WebViewApkApplication.postDeveloperUiLauncherIconTask();
                    }
                });
            }
        }
    }

    @Override
    protected boolean isWebViewProcess() {
        return WebViewApkApplication.isWebViewProcess();
    }
}
