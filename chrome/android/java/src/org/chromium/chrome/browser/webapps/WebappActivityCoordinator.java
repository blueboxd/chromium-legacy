// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;

import javax.inject.Inject;

/**
 * Coordinator shared between webapp activity and WebAPK activity components.
 * Add methods here if other components need to communicate with either of these components.
 */
@ActivityScope
public class WebappActivityCoordinator {
    private final WebappActivity mActivity;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final WebappDeferredStartupWithStorageHandler mDeferredStartupWithStorageHandler;

    @Inject
    public WebappActivityCoordinator(ChromeActivity<?> activity,
            BrowserServicesIntentDataProvider intentDataProvider,
            WebappDeferredStartupWithStorageHandler deferredStartupWithStorageHandler,
            WebappActionsNotificationManager actionsNotificationManager) {
        // We don't need to do anything with |actionsNotificationManager|. We just need to resolve
        // it so that it starts working.

        mActivity = (WebappActivity) activity;
        mIntentDataProvider = intentDataProvider;
        mDeferredStartupWithStorageHandler = deferredStartupWithStorageHandler;

        mDeferredStartupWithStorageHandler.addTask((storage, didCreateStorage) -> {
            if (activity.isActivityFinishingOrDestroyed()) return;

            if (storage != null) {
                updateStorage(storage);
            }
        });
    }

    /**
     * Invoked to add deferred startup tasks to queue.
     */
    public void initDeferredStartupForActivity() {
        mDeferredStartupWithStorageHandler.initDeferredStartupForActivity();
    }

    private void updateStorage(@NonNull WebappDataStorage storage) {
        // The information in the WebappDataStorage may have been purged by the
        // user clearing their history or not launching the web app recently.
        // Restore the data if necessary.
        WebappInfo webappInfo = mActivity.getWebappInfo();
        storage.updateFromWebappInfo(webappInfo);

        // A recent last used time is the indicator that the web app is still
        // present on the home screen, and enables sources such as notifications to
        // launch web apps. Thus, we do not update the last used time when the web
        // app is not directly launched from the home screen, as this interferes
        // with the heuristic.
        if (webappInfo.isLaunchedFromHomescreen()) {
            // TODO(yusufo): WebappRegistry#unregisterOldWebapps uses this information to delete
            // WebappDataStorage objects for legacy webapps which haven't been used in a while.
            storage.updateLastUsedTime();
        }
    }
}
