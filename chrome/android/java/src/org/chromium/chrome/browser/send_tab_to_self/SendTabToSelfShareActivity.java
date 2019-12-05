// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfMetrics.SendTabToSelfShareClickResult;
import org.chromium.chrome.browser.share.ShareActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.NavigationEntry;

/**
 * A simple activity that allows Chrome to expose send tab to self as an option in the share menu.
 */
public class SendTabToSelfShareActivity extends ShareActivity {
    private static BottomSheetContent sBottomSheetContentForTesting;

    @Override
    protected void handleShareAction(ChromeActivity triggeringActivity) {
        Tab tab = triggeringActivity.getActivityTabProvider().get();
        if (tab == null) return;
        NavigationEntry entry = tab.getWebContents().getNavigationController().getVisibleEntry();
        actionHandler(triggeringActivity, entry, triggeringActivity.getBottomSheetController());
    }

    public static void actionHandler(
            Context context, NavigationEntry entry, BottomSheetController controller) {
        if (entry == null || controller == null) {
            return;
        }

        SendTabToSelfShareClickResult.recordClickResult(
                SendTabToSelfShareClickResult.ClickType.SHOW_DEVICE_LIST);
        controller.requestShowContent(createBottomSheetContent(context, entry, controller), true);
        // TODO(crbug.com/968246): Remove the need to call this explicitly and instead have it
        // automatically show since PeekStateEnabled is set to false.
        controller.expandSheet();
    }

    static BottomSheetContent createBottomSheetContent(
            Context context, NavigationEntry entry, BottomSheetController controller) {
        if (sBottomSheetContentForTesting != null) {
            return sBottomSheetContentForTesting;
        }
        return new DevicePickerBottomSheetContent(context, entry, controller);
    }

    public static boolean featureIsAvailable(Tab currentTab) {
        boolean shouldShow =
                SendTabToSelfAndroidBridge.isFeatureAvailable(currentTab.getWebContents());
        if (shouldShow) {
            SendTabToSelfShareClickResult.recordClickResult(
                    SendTabToSelfShareClickResult.ClickType.SHOW_ITEM);
        }
        return shouldShow;
    }

    @VisibleForTesting
    public static void setBottomSheetContentForTesting(BottomSheetContent bottomSheetContent) {
        sBottomSheetContentForTesting = bottomSheetContent;
    }
}
