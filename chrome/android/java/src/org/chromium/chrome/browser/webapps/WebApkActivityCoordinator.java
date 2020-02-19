// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.metrics.WebApkUma;

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
            Lazy<WebApkUpdateManager> webApkUpdateManager) {
        // We don't need to do anything with |disclosureSnackbarController|. We just need to resolve
        // it so that it starts working.

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

        WebApkInfo info = mActivity.getWebApkInfo();
        WebApkUma.recordShellApkVersion(info.shellApkVersion(), info.distributor());
        storage.incrementLaunchCount();

        mWebApkUpdateManager.get().updateIfNeeded(storage, info);
    }
}
