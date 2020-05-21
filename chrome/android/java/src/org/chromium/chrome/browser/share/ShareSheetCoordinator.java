// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

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
     * @param controller        The {@link BottomSheetController} for the current activity.
     * @param provider          The {@link ActivityTabProvider} for the current visible tab.
     * @param modelBuilder      The {@link ShareSheetPropertyModelBuilder} for the share sheet.
     * @param prefServiceBridge The {@link PrefServiceBridge} singleton. This provides preferences
     *                          for the Chrome-provided property models.
     */
    ShareSheetCoordinator(BottomSheetController controller, ActivityTabProvider provider,
            ShareSheetPropertyModelBuilder modelBuilder, PrefServiceBridge prefServiceBridge) {
        mBottomSheetController = controller;
        mActivityTabProvider = provider;
        mPropertyModelBuilder = modelBuilder;
        mExcludeFirstParty = false;
        mPrefServiceBridge = prefServiceBridge;
    }

    void showShareSheet(ShareParams params, boolean saveLastUsed, long shareStartTime) {
        Activity activity = params.getWindow().getActivity().get();
        if (activity == null) {
            return;
        }

        ShareSheetBottomSheetContent bottomSheet = new ShareSheetBottomSheetContent(activity);

        mShareStartTime = shareStartTime;
        List<PropertyModel> chromeFeatures = createTopRowPropertyModels(bottomSheet, activity);
        List<PropertyModel> thirdPartyApps =
                createBottomRowPropertyModels(bottomSheet, activity, params, saveLastUsed);

        bottomSheet.createRecyclerViews(chromeFeatures, thirdPartyApps);

        boolean shown = mBottomSheetController.requestShowContent(bottomSheet, true);
        if (shown) {
            long delta = System.currentTimeMillis() - shareStartTime;
            RecordHistogram.recordMediumTimesHistogram(
                    "Sharing.SharingHubAndroid.TimeToShowShareSheet", delta);
        }
    }

    // Used by first party features to share with only non-chrome apps.
    protected void showThirdPartyShareSheet(
            ShareParams params, boolean saveLastUsed, long shareStartTime) {
        mExcludeFirstParty = true;
        showShareSheet(params, saveLastUsed, shareStartTime);
    }

    List<PropertyModel> createTopRowPropertyModels(
            ShareSheetBottomSheetContent bottomSheet, Activity activity) {
        if (mExcludeFirstParty) {
            return new ArrayList<>();
        }
        ChromeProvidedSharingOptionsProvider chromeProvidedSharingOptionsProvider =
                new ChromeProvidedSharingOptionsProvider(activity, mActivityTabProvider,
                        mBottomSheetController, bottomSheet, mPrefServiceBridge, mShareStartTime);
        return chromeProvidedSharingOptionsProvider.createPropertyModels(new HashSet<>(
                Arrays.asList(ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE,
                        ContentType.TEXT, ContentType.IMAGE, ContentType.OTHER_FILE_TYPE)));
    }

    @VisibleForTesting
    List<PropertyModel> createBottomRowPropertyModels(ShareSheetBottomSheetContent bottomSheet,
            Activity activity, ShareParams params, boolean saveLastUsed) {
        List<PropertyModel> models = mPropertyModelBuilder.selectThirdPartyApps(
                bottomSheet, params, saveLastUsed, mShareStartTime);
        // More...
        PropertyModel morePropertyModel = ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(activity, R.drawable.sharing_more),
                activity.getResources().getString(R.string.sharing_more_icon_label),
                (shareParams)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.MoreSelected");
                    mBottomSheetController.hideContent(bottomSheet, true);
                    ShareHelper.showDefaultShareUi(params, saveLastUsed);
                },
                /*isFirstParty=*/true);
        models.add(morePropertyModel);

        return models;
    }

    @VisibleForTesting
    protected void disableFirstPartyFeaturesForTesting() {
        mExcludeFirstParty = true;
    }

    @IntDef({ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.TEXT,
            ContentType.IMAGE, ContentType.OTHER_FILE_TYPE})
    @Retention(RetentionPolicy.SOURCE)
    @interface ContentType {
        int LINK_PAGE_VISIBLE = 0;
        int LINK_PAGE_NOT_VISIBLE = 1;
        int TEXT = 2;
        int IMAGE = 3;
        int OTHER_FILE_TYPE = 4;
    }
}
