// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
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
    private final PackageManager mPackageManager;
    private static final int MAX_NUM_APPS = 7;

    private static final ArrayList<String> FALLBACK_ACTIVITIES =
            new ArrayList(Arrays.asList("com.whatsapp.ContactPicker",
                    "com.facebook.composer.shareintent.ImplicitShareIntentHandlerDefaultAlias",
                    "com.google.android.gm.ComposeActivityGmailExternal",
                    "com.instagram.share.handleractivity.StoryShareHandlerActivity",
                    "com.facebook.messenger.intents.ShareIntentHandler",
                    "com.google.android.apps.messaging.ui.conversationlist.ShareIntentActivity",
                    "com.twitter.composer.ComposerActivity", "com.snap.mushroom.MainActivity",
                    "com.pinterest.activity.create.PinItActivity",
                    "com.linkedin.android.publishing.sharing.LinkedInDeepLinkActivity",
                    "jp.naver.line.android.activity.selectchat.SelectChatActivityLaunchActivity",
                    "com.facebook.lite.composer.activities.ShareIntentMultiPhotoAlphabeticalAlias",
                    "com.facebook.mlite.share.view.ShareActivity",
                    "com.samsung.android.email.ui.messageview.MessageFileView",
                    "com.yahoo.mail.ui.activities.ComposeActivity",
                    "org.telegram.ui.LaunchActivity", "com.tencent.mm.ui.tools.ShareImgUI"));

    /** Variations parameter name for the comma-separated list of third-party activity names. */
    private static final String PARAM_SHARING_HUB_THIRD_PARTY_APPS = "sharing-hub-third-party-apps";

    ShareSheetPropertyModelBuilder(
            BottomSheetController bottomSheetController, PackageManager packageManager) {
        mBottomSheetController = bottomSheetController;
        mPackageManager = packageManager;
    }

    protected ArrayList<PropertyModel> selectThirdPartyApps(
            ShareSheetBottomSheetContent bottomSheet, ShareParams params) {
        List<String> thirdPartyActivityNames = getThirdPartyActivityNames();
        Intent intent = ShareHelper.getShareLinkAppCompatibilityIntent();
        final ShareHelper.TargetChosenCallback callback = params.getCallback();
        List<ResolveInfo> resolveInfoList = mPackageManager.queryIntentActivities(intent, 0);
        List<ResolveInfo> thirdPartyActivities = new ArrayList<>();

        // Construct a list of 3P apps. The list should be sorted by the country-specific ranking
        // when available or the fallback list defined above.  If less than MAX_NUM_APPS are
        // available the list is filled with whatever else is available.
        for (String s : thirdPartyActivityNames) {
            for (ResolveInfo res : resolveInfoList) {
                if (res.activityInfo.name.equals(s)) {
                    thirdPartyActivities.add(res);
                    resolveInfoList.remove(res);
                    break;
                }
            }
            if (thirdPartyActivities.size() == MAX_NUM_APPS) {
                break;
            }
        }
        for (ResolveInfo res : resolveInfoList) {
            thirdPartyActivities.add(res);
            if (thirdPartyActivities.size() == MAX_NUM_APPS) {
                break;
            }
        }

        ArrayList<PropertyModel> models = new ArrayList<>();
        for (int i = 0; i < MAX_NUM_APPS && i < thirdPartyActivities.size(); ++i) {
            ResolveInfo info = thirdPartyActivities.get(i);
            final int logIndex = i;
            PropertyModel propertyModel =
                    createPropertyModel(ShareHelper.loadIconForResolveInfo(info, mPackageManager),
                            (String) info.loadLabel(mPackageManager), (shareParams) -> {
                                RecordUserAction.record("SharingHubAndroid.ThirdPartyAppSelected");
                                RecordHistogram.recordEnumeratedHistogram(
                                        "Sharing.SharingHubAndroid.ThirdPartyAppUsage", logIndex,
                                        MAX_NUM_APPS + 1);
                                ShareSheetCoordinator.recordTimeToShare();
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

    private List<String> getThirdPartyActivityNames() {
        String param = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.CHROME_SHARING_HUB, PARAM_SHARING_HUB_THIRD_PARTY_APPS);
        if (param.isEmpty()) {
            return FALLBACK_ACTIVITIES;
        }
        return new ArrayList<String>(Arrays.asList(param.split(",")));
    }
}
