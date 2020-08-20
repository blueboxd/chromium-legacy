// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeAccessorActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.ui.base.WindowAndroid;

/**
 * A simple activity that allows Chrome to expose send tab to self as an option in the share menu.
 */
public class SendTabToSelfShareActivity extends ChromeAccessorActivity {
    private static BottomSheetContent sBottomSheetContentForTesting;
    private static BottomSheetController sBottomSheetControllerForTesting;

    @Override
    protected void handleAction(ChromeActivity triggeringActivity) {
        Tab tab = triggeringActivity.getActivityTabProvider().get();
        if (tab == null) return;
        NavigationEntry entry = tab.getWebContents().getNavigationController().getVisibleEntry();
        if (entry == null) return;
        actionHandler(triggeringActivity, entry.getUrl(), entry.getTitle(), entry.getTimestamp(),
                getBottomSheetController(triggeringActivity.getWindowAndroid()));
    }

    public static void actionHandler(Context context, String url, String title, long navigationTime,
            BottomSheetController controller) {
        if (controller == null) {
            return;
        }

        controller.requestShowContent(
                createBottomSheetContent(context, url, title, navigationTime, controller), true);
        // TODO(crbug.com/968246): Remove the need to call this explicitly and instead have it
        // automatically show since PeekStateEnabled is set to false.
        controller.expandSheet();
    }

    static BottomSheetContent createBottomSheetContent(Context context, String url, String title,
            long navigationTime, BottomSheetController controller) {
        if (sBottomSheetContentForTesting != null) {
            return sBottomSheetContentForTesting;
        }
        return new DevicePickerBottomSheetContent(context, url, title, navigationTime, controller);
    }

    public static boolean featureIsAvailable(Tab currentTab) {
        return SendTabToSelfAndroidBridge.isFeatureAvailable(currentTab.getWebContents());
    }

    private BottomSheetController getBottomSheetController(WindowAndroid window) {
        if (sBottomSheetControllerForTesting != null) return sBottomSheetControllerForTesting;
        return BottomSheetControllerProvider.from(window);
    }

    @VisibleForTesting
    public static void setBottomSheetControllerForTesting(BottomSheetController controller) {
        sBottomSheetControllerForTesting = controller;
    }

    @VisibleForTesting
    public static void setBottomSheetContentForTesting(BottomSheetContent bottomSheetContent) {
        sBottomSheetContentForTesting = bottomSheetContent;
    }
}
