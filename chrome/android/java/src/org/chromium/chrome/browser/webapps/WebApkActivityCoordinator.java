// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Coordinator for the WebAPK activity component.
 * Add methods here if other components need to communicate with the WebAPK activity component.
 */
@ActivityScope
public class WebApkActivityCoordinator {
    private final WebApkActivity mActivity;
    private final Lazy<WebApkUpdateManager> mWebApkUpdateManager;

    @Inject
    public WebApkActivityCoordinator(ChromeActivity<?> activity,
            WebappDeferredStartupWithStorageHandler deferredStartupWithStorageHandler,
            WebappDisclosureSnackbarController disclosureSnackbarController,
            WebApkActivityLifecycleUmaTracker webApkActivityLifecycleUmaTracker,
            Lazy<WebApkUpdateManager> webApkUpdateManager) {
        // We don't need to do anything with |disclosureSnackbarController| and
        // |webApkActivityLifecycleUmaTracker|. We just need to resolve
        // them so that they start working.

        mActivity = (WebApkActivity) activity;
        mWebApkUpdateManager = webApkUpdateManager;

        deferredStartupWithStorageHandler.addTask((storage, didCreateStorage) -> {
            if (activity.isActivityFinishingOrDestroyed()) return;

            onDeferredStartupWithStorage(storage, didCreateStorage);
        });
    }

    public void onDeferredStartupWithStorage(
            @Nullable WebappDataStorage storage, boolean didCreateStorage) {
        assert storage != null;
        storage.incrementLaunchCount();

        WebApkInfo info = (WebApkInfo) mActivity.getWebappInfo();
        mWebApkUpdateManager.get().updateIfNeeded(storage, info);
    }
}
