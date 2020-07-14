// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.text.TextUtils;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfShareActivity;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.qrcode.QrCodeCoordinator;
import org.chromium.chrome.browser.share.screenshot.ScreenshotCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * Provides {@code PropertyModel}s of Chrome-provided sharing options.
 */
class ChromeProvidedSharingOptionsProvider {
    private final Activity mActivity;
    private final Supplier<Tab> mTabProvider;
    private final BottomSheetController mBottomSheetController;
    private final ShareSheetBottomSheetContent mBottomSheetContent;
    private final PrefServiceBridge mPrefServiceBridge;
    private final ShareParams mShareParams;
    private final Callback<Tab> mPrintTabCallback;
    private final long mShareStartTime;
    private final List<FirstPartyOption> mOrderedFirstPartyOptions;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private ScreenshotCoordinator mScreenshotCoordinator;
    private final String mUrl;

    /**
     * Constructs a new {@link ChromeProvidedSharingOptionsProvider}.
     *
     * @param activity The current {@link Activity}.
     * @param tabProvider Supplier for the current activity tab.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param bottomSheetContent The {@link ShareSheetBottomSheetContent} for the current
     * activity.
     * @param prefServiceBridge The {@link PrefServiceBridge} singleton. This provides printing
     * preferences.
     * @param shareParams The {@link ShareParams} for the current share.
     * @param chromeShareExtras The {@link ChromeShareExtras} for the current share.
     * @param printTab A {@link Callback} that will print a given Tab.
     * @param shareStartTime The start time of the current share.
     * @param chromeOptionShareCallback A ChromeOptionShareCallback that can be used by
     * Chrome-provided sharing options.
     */
    ChromeProvidedSharingOptionsProvider(Activity activity, Supplier<Tab> tabProvider,
            BottomSheetController bottomSheetController,
            ShareSheetBottomSheetContent bottomSheetContent, PrefServiceBridge prefServiceBridge,
            ShareParams shareParams, ChromeShareExtras chromeShareExtras, Callback<Tab> printTab,
            long shareStartTime, ChromeOptionShareCallback chromeOptionShareCallback) {
        mActivity = activity;
        mTabProvider = tabProvider;
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mPrefServiceBridge = prefServiceBridge;
        mShareParams = shareParams;
        mPrintTabCallback = printTab;
        mShareStartTime = shareStartTime;
        mOrderedFirstPartyOptions = new ArrayList<>();
        initializeFirstPartyOptionsInOrder();
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mUrl = getUrlToShare(shareParams, chromeShareExtras);
    }

    /**
     * Encapsulates a {@link PropertyModel} and the {@link ContentType}s it should be shown for.
     */
    private static class FirstPartyOption {
        private final PropertyModel mPropertyModel;
        private final Collection<Integer> mContentTypes;

        FirstPartyOption(PropertyModel propertyModel, Collection<Integer> contentTypes) {
            mPropertyModel = propertyModel;
            mContentTypes = contentTypes;
        }

        PropertyModel getPropertyModel() {
            return mPropertyModel;
        }

        Collection<Integer> getContentTypes() {
            return mContentTypes;
        }
    }

    /**
     * Returns an ordered list of {@link PropertyModel}s that should be shown given the {@code
     * contentTypes} being shared.
     *
     * @param contentTypes a {@link Set} of {@link ContentType}.
     * @return a list of {@link PropertyModel}s.
     */
    List<PropertyModel> getPropertyModels(Set<Integer> contentTypes) {
        List<PropertyModel> propertyModels = new ArrayList<>();
        for (FirstPartyOption firstPartyOption : mOrderedFirstPartyOptions) {
            if (!Collections.disjoint(contentTypes, firstPartyOption.getContentTypes())) {
                propertyModels.add(firstPartyOption.getPropertyModel());
            }
        }
        return propertyModels;
    }

    /**
     * Creates all enabled {@link FirstPartyOption}s and adds them to {@code
     * mOrderedFirstPartyOptions} in the order they should appear.
     */
    private void initializeFirstPartyOptionsInOrder() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_SCREENSHOT)) {
            mOrderedFirstPartyOptions.add(createScreenshotFirstPartyOption());
        }
        mOrderedFirstPartyOptions.add(createCopyLinkFirstPartyOption());
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)) {
            mOrderedFirstPartyOptions.add(createCopyTextFirstPartyOption());
        }
        mOrderedFirstPartyOptions.add(createSendTabToSelfFirstPartyOption());
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_QRCODE)) {
            mOrderedFirstPartyOptions.add(createQrCodeFirstPartyOption());
        }
        if (mPrefServiceBridge.getBoolean(Pref.PRINTING_ENABLED)) {
            mOrderedFirstPartyOptions.add(createPrintingFirstPartyOption());
        }
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

    private FirstPartyOption createScreenshotFirstPartyOption() {
        PropertyModel propertyModel = ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.screenshot),
                mActivity.getResources().getString(R.string.sharing_screenshot),
                (view)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.ScreenshotSelected");
                    RecordHistogram.recordMediumTimesHistogram(
                            "Sharing.SharingHubAndroid.TimeToShare",
                            System.currentTimeMillis() - mShareStartTime);
                    mScreenshotCoordinator = new ScreenshotCoordinator(mActivity,
                            mTabProvider.get(), mChromeOptionShareCallback, mBottomSheetController);
                    // Capture a screenshot once the bottom sheet is fully hidden. The
                    // observer will then remove itself.
                    mBottomSheetController.addObserver(mSheetObserver);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                },
                /*isFirstParty=*/true);
        return new FirstPartyOption(propertyModel,
                Arrays.asList(ContentType.LINK_PAGE_VISIBLE, ContentType.TEXT, ContentType.IMAGE));
    }

    private FirstPartyOption createCopyLinkFirstPartyOption() {
        PropertyModel propertyModel = ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.ic_content_copy_black),
                mActivity.getResources().getString(R.string.sharing_copy_url),
                (view)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.CopyURLSelected");
                    RecordHistogram.recordMediumTimesHistogram(
                            "Sharing.SharingHubAndroid.TimeToShare",
                            System.currentTimeMillis() - mShareStartTime);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                    ClipboardManager clipboard = (ClipboardManager) mActivity.getSystemService(
                            Context.CLIPBOARD_SERVICE);
                    clipboard.setPrimaryClip(
                            ClipData.newPlainText(mShareParams.getTitle(), mShareParams.getUrl()));
                    Toast.makeText(mActivity, R.string.link_copied, Toast.LENGTH_SHORT).show();
                },
                /*isFirstParty=*/true);
        return new FirstPartyOption(propertyModel,
                Arrays.asList(ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE));
    }

    private FirstPartyOption createCopyTextFirstPartyOption() {
        PropertyModel propertyModel = ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.ic_content_copy_black),
                mActivity.getResources().getString(R.string.sharing_copy_text),
                (view)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.CopyTextSelected");
                    RecordHistogram.recordMediumTimesHistogram(
                            "Sharing.SharingHubAndroid.TimeToShare",
                            System.currentTimeMillis() - mShareStartTime);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                    ClipboardManager clipboard = (ClipboardManager) mActivity.getSystemService(
                            Context.CLIPBOARD_SERVICE);
                    clipboard.setPrimaryClip(
                            ClipData.newPlainText(mShareParams.getTitle(), mShareParams.getText()));
                    Toast.makeText(mActivity, R.string.text_copied, Toast.LENGTH_SHORT).show();
                },
                /*isFirstParty=*/true);
        return new FirstPartyOption(propertyModel, Collections.singleton(ContentType.TEXT));
    }

    private FirstPartyOption createSendTabToSelfFirstPartyOption() {
        PropertyModel propertyModel = ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.send_tab),
                mActivity.getResources().getString(R.string.send_tab_to_self_share_activity_title),
                (view)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.SendTabToSelfSelected");
                    RecordHistogram.recordMediumTimesHistogram(
                            "Sharing.SharingHubAndroid.TimeToShare",
                            System.currentTimeMillis() - mShareStartTime);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                    SendTabToSelfShareActivity.actionHandler(mActivity, mUrl,
                            mShareParams.getTitle(),
                            mTabProvider.get()
                                    .getWebContents()
                                    .getNavigationController()
                                    .getVisibleEntry()
                                    .getTimestamp(),
                            mBottomSheetController);
                },
                /*isFirstParty=*/true);
        return new FirstPartyOption(propertyModel,
                Arrays.asList(ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE,
                        ContentType.IMAGE));
    }

    private FirstPartyOption createQrCodeFirstPartyOption() {
        PropertyModel propertyModel = ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.qr_code),
                mActivity.getResources().getString(R.string.qr_code_share_icon_label),
                (view)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.QRCodeSelected");
                    RecordHistogram.recordMediumTimesHistogram(
                            "Sharing.SharingHubAndroid.TimeToShare",
                            System.currentTimeMillis() - mShareStartTime);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                    QrCodeCoordinator qrCodeCoordinator = new QrCodeCoordinator(mActivity, mUrl);
                    qrCodeCoordinator.show();
                },
                /*isFirstParty=*/true);
        return new FirstPartyOption(propertyModel,
                Arrays.asList(ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE,
                        ContentType.IMAGE));
    }

    private FirstPartyOption createPrintingFirstPartyOption() {
        PropertyModel propertyModel = ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.sharing_print),
                mActivity.getResources().getString(R.string.print_share_activity_title),
                (view)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.PrintSelected");
                    RecordHistogram.recordMediumTimesHistogram(
                            "Sharing.SharingHubAndroid.TimeToShare",
                            System.currentTimeMillis() - mShareStartTime);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                    mPrintTabCallback.onResult(mTabProvider.get());
                },
                /*isFirstParty=*/true);
        return new FirstPartyOption(
                propertyModel, Collections.singleton(ContentType.LINK_PAGE_VISIBLE));
    }

    /**
     * Returns the url to share.
     *
     * <p>This prioritizes the URL in {@link ShareParams}, but if it does not exist, we look for an
     * image source URL from {@link ChromeShareExtras}. The image source URL is not contained in
     * {@link ShareParams#getUrl()} because we do not want to share the image URL with the image
     * file in third-party app shares.
     */
    static String getUrlToShare(ShareParams shareParams, ChromeShareExtras chromeShareExtras) {
        if (!TextUtils.isEmpty(shareParams.getUrl())) {
            return shareParams.getUrl();
        }
        return chromeShareExtras.getImageSrcUrl();
    }
}
