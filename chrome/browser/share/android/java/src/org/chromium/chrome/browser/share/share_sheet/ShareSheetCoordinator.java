// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Coordinator for displaying the share sheet.
 */
// TODO(crbug/1022172): Should be package-protected once modularization is complete.
public class ShareSheetCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final Supplier<Tab> mTabProvider;
    private final ShareSheetPropertyModelBuilder mPropertyModelBuilder;
    private final PrefServiceBridge mPrefServiceBridge;
    private final Callback<Tab> mPrintTabCallback;
    private long mShareStartTime;
    private boolean mExcludeFirstParty;

    /**
     * Constructs a new ShareSheetCoordinator.
     *
     * @param controller The {@link BottomSheetController} for the current activity.
     * @param tabProvider Supplier for the current activity tab.
     * @param modelBuilder The {@link ShareSheetPropertyModelBuilder} for the share sheet.
     * @param prefServiceBridge The {@link PrefServiceBridge} singleton. This provides preferences
     * for the Chrome-provided property models.
     */
    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    public ShareSheetCoordinator(BottomSheetController controller, Supplier<Tab> tabProvider,
            ShareSheetPropertyModelBuilder modelBuilder, PrefServiceBridge prefServiceBridge,
            Callback<Tab> printTab) {
        mBottomSheetController = controller;
        mTabProvider = tabProvider;
        mPropertyModelBuilder = modelBuilder;
        mExcludeFirstParty = false;
        mPrefServiceBridge = prefServiceBridge;
        mPrintTabCallback = printTab;
    }

    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    public void showShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        Activity activity = params.getWindow().getActivity().get();
        if (activity == null) {
            return;
        }

        ShareSheetBottomSheetContent bottomSheet = new ShareSheetBottomSheetContent(activity);

        mShareStartTime = shareStartTime;
        Set<Integer> contentTypes = ShareSheetPropertyModelBuilder.getContentTypes(
                params, chromeShareExtras.isUrlOfVisiblePage());
        List<PropertyModel> chromeFeatures =
                createTopRowPropertyModels(bottomSheet, activity, params, contentTypes);
        List<PropertyModel> thirdPartyApps = createBottomRowPropertyModels(
                bottomSheet, activity, params, contentTypes, chromeShareExtras.saveLastUsed());

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
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        mExcludeFirstParty = true;
        showShareSheet(params, chromeShareExtras, shareStartTime);
    }

    List<PropertyModel> createTopRowPropertyModels(ShareSheetBottomSheetContent bottomSheet,
            Activity activity, ShareParams shareParams, Set<Integer> contentTypes) {
        if (mExcludeFirstParty) {
            return new ArrayList<>();
        }
        ChromeProvidedSharingOptionsProvider chromeProvidedSharingOptionsProvider =
                new ChromeProvidedSharingOptionsProvider(activity, mTabProvider,
                        mBottomSheetController, bottomSheet, mPrefServiceBridge, shareParams,
                        mPrintTabCallback, mShareStartTime);

        return chromeProvidedSharingOptionsProvider.getPropertyModels(contentTypes);
    }

    @VisibleForTesting
    List<PropertyModel> createBottomRowPropertyModels(ShareSheetBottomSheetContent bottomSheet,
            Activity activity, ShareParams params, Set<Integer> contentTypes,
            boolean saveLastUsed) {
        List<PropertyModel> models = mPropertyModelBuilder.selectThirdPartyApps(
                bottomSheet, contentTypes, params, saveLastUsed, mShareStartTime);
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
}
