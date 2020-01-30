// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Handles displaying the share sheet. The version used depends on several
 * conditions.
 * Android K and below: custom share dialog
 * Android L+: system share sheet
 * #chrome-sharing-hub enabled: custom share sheet
 */
class ShareSheetPropertyModelBuilder {
    private final BottomSheetController mBottomSheetController;

    ShareSheetPropertyModelBuilder(BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
    }

    protected ArrayList<PropertyModel> selectThirdPartyApps(
            Activity activity, ShareSheetBottomSheetContent bottomSheet, ShareParams params) {
        Intent intent = ShareHelper.getShareLinkAppCompatibilityIntent();
        final ShareHelper.TargetChosenCallback callback = params.getCallback();
        PackageManager manager = activity.getPackageManager();
        List<ResolveInfo> resolveInfoList = manager.queryIntentActivities(intent, 0);
        List<ResolveInfo> thirdPartyActivities = new ArrayList<>();
        for (ResolveInfo res : resolveInfoList) {
            if (!res.activityInfo.packageName.equals(activity.getPackageName())) {
                thirdPartyActivities.add(res);
            }
        }

        ArrayList<PropertyModel> models = new ArrayList<>();
        for (int i = 0; i < 7 && i < thirdPartyActivities.size(); ++i) {
            ResolveInfo info = thirdPartyActivities.get(i);
            PropertyModel propertyModel =
                    createPropertyModel(ShareHelper.loadIconForResolveInfo(info, manager),
                            (String) info.loadLabel(manager), (shareParams) -> {
                                ActivityInfo ai = info.activityInfo;

                                ComponentName component =
                                        new ComponentName(ai.applicationInfo.packageName, ai.name);
                                if (callback != null) {
                                    callback.onTargetChosen(component);
                                }
                                if (params.saveLastUsed()) {
                                    ShareHelper.setLastShareComponentName(
                                            component, params.getSourcePackageName());
                                }
                                mBottomSheetController.hideContent(bottomSheet, true);
                                ShareHelper.makeIntentAndShare(params, component);
                            }, /*isFirstParty=*/false);
            models.add(propertyModel);
        }
        return models;
    }

    protected PropertyModel createPropertyModel(
            Drawable icon, String label, OnClickListener listener, boolean isFirstParty) {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                        .with(ShareSheetItemViewProperties.ICON, icon)
                        .with(ShareSheetItemViewProperties.LABEL, label)
                        .with(ShareSheetItemViewProperties.CLICK_LISTENER, listener)
                        .with(ShareSheetItemViewProperties.IS_FIRST_PARTY, isFirstParty)
                        .build();
        return propertyModel;
    }
}
