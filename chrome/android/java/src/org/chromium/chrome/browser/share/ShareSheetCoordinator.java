// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

/**
 * Coordinator for displaying the share sheet.
 */
class ShareSheetCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final ActivityTabProvider mActivityTabProvider;
    private final ShareSheetPropertyModelBuilder mPropertyModelBuilder;
    private final PrefServiceBridge mPrefServiceBridge;
    private long mShareStartTime;
    private boolean mExcludeFirstParty;

    /**
     * Constructs a new ShareSheetCoordinator.
     *
     * @param controller The BottomSheetController for the current activity.
     * @param provider   The ActivityTabProvider for the current visible tab.
     */
    ShareSheetCoordinator(BottomSheetController controller, ActivityTabProvider provider,
            ShareSheetPropertyModelBuilder modelBuilder, PrefServiceBridge prefServiceBridge) {
        mBottomSheetController = controller;
        mActivityTabProvider = provider;
        mPropertyModelBuilder = modelBuilder;
        mPrefServiceBridge = prefServiceBridge;
        mExcludeFirstParty = false;
    }

    void showShareSheet(ShareParams params, long shareStartTime) {
        Activity activity = params.getWindow().getActivity().get();
        if (activity == null) {
            return;
        }

        ShareSheetBottomSheetContent bottomSheet = new ShareSheetBottomSheetContent(activity);

        mShareStartTime = shareStartTime;
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

    // Used by first party features to share with only non-chrome apps.
    protected void showThirdPartyShareSheet(ShareParams params) {
        mExcludeFirstParty = true;
        showShareSheet(params, System.currentTimeMillis());
    }

    @VisibleForTesting
    ArrayList<PropertyModel> createTopRowPropertyModels(
            ShareSheetBottomSheetContent bottomSheet, Activity activity) {
        ChromeProvidedSharingOptionsProvider chromeProvidedSharingOptionsProvider =
                new ChromeProvidedSharingOptionsProvider(activity, mActivityTabProvider,
                        mBottomSheetController, bottomSheet, mShareStartTime);
        ArrayList<PropertyModel> models = new ArrayList<>();
        if (mExcludeFirstParty) {
            return models;
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_SCREENSHOT)) {
            models.add(chromeProvidedSharingOptionsProvider.createScreenshotPropertyModel());
        }
        models.add(chromeProvidedSharingOptionsProvider.createCopyLinkPropertyModel());
        models.add(chromeProvidedSharingOptionsProvider.createSendTabToSelfPropertyModel());
        models.add(chromeProvidedSharingOptionsProvider.createQrCodePropertyModel());
        if (mPrefServiceBridge.getBoolean(Pref.PRINTING_ENABLED)) {
            models.add(chromeProvidedSharingOptionsProvider.createPrintingPropertyModel());
        }

        return models;
    }

    @VisibleForTesting
    ArrayList<PropertyModel> createBottomRowPropertyModels(
            ShareSheetBottomSheetContent bottomSheet, Activity activity, ShareParams params) {
        ArrayList<PropertyModel> models =
                mPropertyModelBuilder.selectThirdPartyApps(bottomSheet, params, mShareStartTime);
        // More...
        PropertyModel morePropertyModel = ShareSheetPropertyModelBuilder.createPropertyModel(
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

    @VisibleForTesting
    protected void disableFirstPartyFeaturesForTesting() {
        mExcludeFirstParty = true;
    }
}
