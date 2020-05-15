// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.printing.TabPrinter;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfShareActivity;
import org.chromium.chrome.browser.share.qrcode.QrCodeCoordinator;
import org.chromium.chrome.browser.share.screenshot.ScreenshotCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.SheetState;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

/**
 * Provides {@code PropertyModel}s of Chrome-provided sharing options.
 */
class ChromeProvidedSharingOptionsProvider {
    private final Activity mActivity;
    private final ActivityTabProvider mActivityTabProvider;
    private final BottomSheetController mBottomSheetController;
    private final ShareSheetBottomSheetContent mBottomSheetContent;
    private final long mShareStartTime;
    private ScreenshotCoordinator mScreenshotCoordinator;

    /**
     * Constructs a new {@link ChromeProvidedSharingOptionsProvider}.
     *
     * @param activity              The current {@link Activity}.
     * @param activityTabProvider   The {@link ActivityTabProvider} for the current visible tab.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param bottomSheetContent    The {@link ShareSheetBottomSheetContent} for the current
     *                              activity.
     * @param shareStartTime        The start time of the current share.
     */
    ChromeProvidedSharingOptionsProvider(Activity activity, ActivityTabProvider activityTabProvider,
            BottomSheetController bottomSheetController,
            ShareSheetBottomSheetContent bottomSheetContent, long shareStartTime) {
        mActivity = activity;
        mActivityTabProvider = activityTabProvider;
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mShareStartTime = shareStartTime;
    }

    /**
     * Used to initiate the screenshot flow once the bottom sheet is fully hidden. Removes itself
     * from {@link BottomSheetController} afterwards.
     */
    private final BottomSheetObserver mSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetStateChanged(int newState) {
            if (newState == SheetState.HIDDEN) {
                assert mScreenshotCoordinator != null;
                mScreenshotCoordinator.captureScreenshot();
                // Clean up the observer since the coordinator is discarded when sheet is hidden.
                mBottomSheetController.removeObserver(mSheetObserver);
            }
        }
    };

    PropertyModel createScreenshotPropertyModel() {
        return ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.screenshot),
                mActivity.getResources().getString(R.string.sharing_screenshot),
                (shareParams)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.ScreenshotSelected");
                    RecordHistogram.recordMediumTimesHistogram(
                            "Sharing.SharingHubAndroid.TimeToShare",
                            System.currentTimeMillis() - mShareStartTime);
                    mScreenshotCoordinator =
                            new ScreenshotCoordinator(mActivity, mActivityTabProvider.get());
                    // Capture a screenshot once the bottom sheet is fully hidden. The observer
                    // will then remove itself.
                    mBottomSheetController.addObserver(mSheetObserver);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                },
                /*isFirstParty=*/true);
    }

    PropertyModel createCopyLinkPropertyModel() {
        return ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.ic_content_copy_black),
                mActivity.getResources().getString(R.string.sharing_copy_url), (shareParams) -> {
                    RecordUserAction.record("SharingHubAndroid.CopyURLSelected");
                    RecordHistogram.recordMediumTimesHistogram(
                            "Sharing.SharingHubAndroid.TimeToShare",
                            System.currentTimeMillis() - mShareStartTime);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                    Tab tab = mActivityTabProvider.get();
                    NavigationEntry entry =
                            tab.getWebContents().getNavigationController().getVisibleEntry();
                    ClipboardManager clipboard = (ClipboardManager) mActivity.getSystemService(
                            Context.CLIPBOARD_SERVICE);
                    ClipData clip = ClipData.newPlainText(entry.getTitle(), entry.getUrl());
                    clipboard.setPrimaryClip(clip);
                    Toast toast =
                            Toast.makeText(mActivity, R.string.link_copied, Toast.LENGTH_SHORT);
                    toast.show();
                }, /*isFirstParty=*/true);
    }

    PropertyModel createSendTabToSelfPropertyModel() {
        return ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.send_tab),
                mActivity.getResources().getString(R.string.send_tab_to_self_share_activity_title),
                (shareParams)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.SendTabToSelfSelected");
                    RecordHistogram.recordMediumTimesHistogram(
                            "Sharing.SharingHubAndroid.TimeToShare",
                            System.currentTimeMillis() - mShareStartTime);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                    SendTabToSelfShareActivity.actionHandler(mActivity,
                            mActivityTabProvider.get()
                                    .getWebContents()
                                    .getNavigationController()
                                    .getVisibleEntry(),
                            mBottomSheetController, mActivityTabProvider.get().getWebContents());
                },
                /*isFirstParty=*/true);
    }

    PropertyModel createQrCodePropertyModel() {
        return ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.qr_code),
                mActivity.getResources().getString(R.string.qr_code_share_icon_label),
                (currentActivity)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.QRCodeSelected");
                    RecordHistogram.recordMediumTimesHistogram(
                            "Sharing.SharingHubAndroid.TimeToShare",
                            System.currentTimeMillis() - mShareStartTime);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                    QrCodeCoordinator qrCodeCoordinator = new QrCodeCoordinator(mActivity);
                    qrCodeCoordinator.show();
                },
                /*isFirstParty=*/true);
    }

    PropertyModel createPrintingPropertyModel() {
        return ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.sharing_print),
                mActivity.getResources().getString(R.string.print_share_activity_title),
                (currentActivity)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.PrintSelected");
                    RecordHistogram.recordMediumTimesHistogram(
                            "Sharing.SharingHubAndroid.TimeToShare",
                            System.currentTimeMillis() - mShareStartTime);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                    PrintingController printingController = PrintingControllerImpl.getInstance();
                    if (printingController != null && !printingController.isBusy()) {
                        printingController.startPrint(new TabPrinter(mActivityTabProvider.get()),
                                new PrintManagerDelegateImpl(mActivity));
                    }
                },
                /*isFirstParty=*/true);
    }
}