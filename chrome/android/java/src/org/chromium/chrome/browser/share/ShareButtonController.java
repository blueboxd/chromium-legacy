// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.View.OnClickListener;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarVariationManager;

/**
 * Handles displaying share button on toolbar depending on several conditions (e.g.,device width,
 * whether NTP is shown).
 */
public class ShareButtonController implements ButtonDataProvider {
    /**
     * Default minimum width to show the share button.
     */
    public static final int MIN_WIDTH_DP = 360;

    // Context is used for fetching resources and launching preferences page.
    private final Context mContext;

    private final ShareUtils mShareUtils;

    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;

    private final ComponentCallbacks mComponentCallbacks;

    // The activity tab provider.
    private ActivityTabProvider mTabProvider;

    private ButtonData mButtonData;
    private ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();
    private final ObservableSupplier<Boolean> mBottomToolbarVisibilitySupplier;
    private OnClickListener mOnClickListener;

    private Integer mMinimumWidthDp;
    private int mScreenWidthDp;

    private int mCurrentOrientation;

    /**
     * Creates ShareButtonController object.
     * @param context The Context for retrieving resources, etc.
     * @param tabProvider The {@link ActivityTabProvider} used for accessing the tab.
     * @param shareDelegateSupplier The supplier to get a handle on the share delegate.
     * @param shareUtils The share utility functions used by this class.
     * @param bottomToolbarVisibilitySupplier Supplier that queries and updates the visibility of
     * the bottom toolbar.
     */
    public ShareButtonController(Context context, ActivityTabProvider tabProvider,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier, ShareUtils shareUtils,
            ObservableSupplier<Boolean> bottomToolbarVisibilitySupplier) {
        mContext = context;
        mBottomToolbarVisibilitySupplier = bottomToolbarVisibilitySupplier;
        mBottomToolbarVisibilitySupplier.addObserver(
                (bottomToolbarIsVisible)
                        -> notifyObservers(!(bottomToolbarIsVisible
                                && BottomToolbarVariationManager.isShareButtonOnBottom())));

        mTabProvider = tabProvider;
        mShareUtils = shareUtils;

        mShareDelegateSupplier = shareDelegateSupplier;
        mOnClickListener = ((view) -> {
            ShareDelegate shareDelegate = mShareDelegateSupplier.get();
            assert shareDelegate
                    != null : "Share delegate became null after share button was displayed";
            if (shareDelegate == null) return;
            RecordUserAction.record("MobileTopToolbarShareButton");
            Tab tab = mTabProvider.get();
            shareDelegate.share(tab, /*shareDirectly=*/false);
        });

        mComponentCallbacks = new ComponentCallbacks() {
            @Override
            public void onConfigurationChanged(Configuration configuration) {
                int newOrientation = configuration.orientation;
                if (newOrientation == mCurrentOrientation) return;
                mCurrentOrientation = newOrientation;
                if (mCurrentOrientation == Configuration.ORIENTATION_LANDSCAPE) {
                    mScreenWidthDp = Integer.MAX_VALUE;
                } else {
                    mScreenWidthDp = configuration.screenWidthDp;
                }
                updateButtonVisibility(mTabProvider.get());
                notifyObservers(mButtonData.canShow);
            }
            @Override
            public void onLowMemory() {}
        };
        mContext.registerComponentCallbacks(mComponentCallbacks);

        mButtonData = new ButtonData(false,
                AppCompatResources.getDrawable(mContext, R.drawable.ic_toolbar_share_offset_24dp),
                mOnClickListener, R.string.share, true, null);

        mScreenWidthDp = mContext.getResources().getConfiguration().screenWidthDp;
    }

    @Override
    public void destroy() {
        mContext.unregisterComponentCallbacks(mComponentCallbacks);
    }

    @Override
    public void addObserver(ButtonDataObserver obs) {
        mObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(ButtonDataObserver obs) {
        mObservers.removeObserver(obs);
    }

    @Override
    public ButtonData get(Tab tab) {
        updateButtonVisibility(tab);
        return mButtonData;
    }

    private void updateButtonVisibility(Tab tab) {
        if (tab == null || tab.getWebContents() == null
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR)) {
            mButtonData.canShow = false;
            return;
        }

        if (mMinimumWidthDp == null) {
            mMinimumWidthDp = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR, "minimum_width", MIN_WIDTH_DP);
        }

        boolean isDeviceWideEnough = mScreenWidthDp > mMinimumWidthDp;

        if ((mBottomToolbarVisibilitySupplier.get()
                    && BottomToolbarVariationManager.isShareButtonOnBottom())
                || mShareDelegateSupplier.get() == null || !isDeviceWideEnough) {
            mButtonData.canShow = false;
            return;
        }

        mButtonData.canShow = mShareUtils.shouldEnableShare(tab);
    }

    private void notifyObservers(boolean hint) {
        for (ButtonDataObserver observer : mObservers) {
            observer.buttonDataChanged(hint);
        }
    }
}
