// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfShareActivity;
import org.chromium.chrome.browser.share.qrcode.QrCodeCoordinator;
import org.chromium.chrome.browser.share.screenshot.ScreenshotCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;

/**
 * Coordinator for displaying the share sheet.
 */
public class ShareSheetCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final ActivityTabProvider mActivityTabProvider;
    private final ShareSheetPropertyModelBuilder mPropertyModelBuilder;

    /**
     * Constructs a new ShareSheetCoordinator.
     * @param controller The BottomSheetController for the current activity.
     * @param provider The ActivityTabProvider for the current visible tab.
     */
    public ShareSheetCoordinator(BottomSheetController controller, ActivityTabProvider provider,
            ShareSheetPropertyModelBuilder modelBuilder) {
        mBottomSheetController = controller;
        mActivityTabProvider = provider;
        mPropertyModelBuilder = modelBuilder;
    }

    protected void showShareSheet(ShareParams params, long shareStartTime) {
        Activity activity = params.getWindow().getActivity().get();
        if (activity == null) {
            return;
        }

        ShareSheetBottomSheetContent bottomSheet = new ShareSheetBottomSheetContent(activity);

        ArrayList<PropertyModel> chromeFeatures = createTopRowPropertyModels(bottomSheet, activity);
        ArrayList<PropertyModel> thirdPartyApps =
                createBottomRowPropertyModels(bottomSheet, activity, params);

        bottomSheet.createRecyclerViews(chromeFeatures, thirdPartyApps);

        boolean shown = mBottomSheetController.requestShowContent(bottomSheet, true);
        if (shown) {
            long delta = System.currentTimeMillis() - shareStartTime;
            RecordHistogram.recordMediumTimesHistogram(
                    "Sharing.SharingHubAndroid.TimeToShowShareSheet", delta);
        }
    }

    @VisibleForTesting
    ArrayList<PropertyModel> createTopRowPropertyModels(
            ShareSheetBottomSheetContent bottomSheet, Activity activity) {
        ArrayList<PropertyModel> models = new ArrayList<>();
        // ScreenShot
        PropertyModel screenshotPropertyModel = null;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_SCREENSHOT)) {
            screenshotPropertyModel = mPropertyModelBuilder.createPropertyModel(
                    AppCompatResources.getDrawable(activity, R.drawable.screenshot),
                    activity.getResources().getString(R.string.sharing_screenshot),
                    (shareParams)
                            -> {
                        RecordUserAction.record("SharingHubAndroid.ScreenshotSelected");
                        mBottomSheetController.hideContent(bottomSheet, true);
                        Tab tab = mActivityTabProvider.get();
                        ScreenshotCoordinator screenshotCoordinator =
                                new ScreenshotCoordinator(activity, tab);
                        screenshotCoordinator.handleScreenshot();
                    },
                    /*isFirstParty=*/true);
            models.add(screenshotPropertyModel);
        }

        // Copy URL
        PropertyModel copyPropertyModel = mPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(activity, R.drawable.ic_content_copy_black),
                activity.getResources().getString(R.string.sharing_copy_url), (params) -> {
                    RecordUserAction.record("SharingHubAndroid.CopyURLSelected");
                    mBottomSheetController.hideContent(bottomSheet, true);
                    Tab tab = mActivityTabProvider.get();
                    NavigationEntry entry =
                            tab.getWebContents().getNavigationController().getVisibleEntry();
                    ClipboardManager clipboard =
                            (ClipboardManager) activity.getSystemService(Context.CLIPBOARD_SERVICE);
                    ClipData clip = ClipData.newPlainText(entry.getTitle(), entry.getUrl());
                    clipboard.setPrimaryClip(clip);
                    Toast toast =
                            Toast.makeText(activity, R.string.link_copied, Toast.LENGTH_SHORT);
                    toast.show();
                }, /*isFirstParty=*/true);
        models.add(copyPropertyModel);

        // Send Tab To Self
        PropertyModel sttsPropertyModel =
                mPropertyModelBuilder
                        .createPropertyModel(
                                AppCompatResources.getDrawable(activity, R.drawable.send_tab),
                                activity.getResources().getString(
                                        R.string.send_tab_to_self_share_activity_title),
                                (shareParams)
                                        -> {
                                    RecordUserAction.record(
                                            "SharingHubAndroid.SendTabToSelfSelected");
                                    mBottomSheetController.hideContent(bottomSheet, true);
                                    SendTabToSelfShareActivity.actionHandler(activity,
                                            mActivityTabProvider.get()
                                                    .getWebContents()
                                                    .getNavigationController()
                                                    .getVisibleEntry(),
                                            mBottomSheetController,
                                            mActivityTabProvider.get().getWebContents());
                                },
                                /*isFirstParty=*/true);
        models.add(sttsPropertyModel);

        // QR Codes
        PropertyModel qrcodePropertyModel = mPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(activity, R.drawable.qr_code),
                activity.getResources().getString(R.string.qr_code_share_icon_label),
                (currentActivity)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.QRCodeSelected");
                    mBottomSheetController.hideContent(bottomSheet, true);
                    QrCodeCoordinator qrCodeCoordinator = new QrCodeCoordinator(activity);
                    qrCodeCoordinator.show();
                },
                /*isFirstParty=*/true);
        models.add(qrcodePropertyModel);

        return models;
    }

    @VisibleForTesting
    ArrayList<PropertyModel> createBottomRowPropertyModels(
            ShareSheetBottomSheetContent bottomSheet, Activity activity, ShareParams params) {
        ArrayList<PropertyModel> models =
                mPropertyModelBuilder.selectThirdPartyApps(activity, bottomSheet, params);
        // More...
        PropertyModel morePropertyModel = mPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(activity, R.drawable.sharing_more),
                activity.getResources().getString(R.string.sharing_more_icon_label),
                (shareParams)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.MoreSelected");
                    mBottomSheetController.hideContent(bottomSheet, true);
                    ShareHelper.showDefaultShareUi(params);
                },
                /*isFirstParty=*/true);
        models.add(morePropertyModel);

        return models;
    }
}
