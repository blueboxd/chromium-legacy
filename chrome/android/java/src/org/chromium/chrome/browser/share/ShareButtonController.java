// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;
import android.view.View.OnClickListener;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
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
    // Context is used for fetching resources and launching preferences page.
    private final Context mContext;

    private final ShareUtils mShareUtils;

    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;

    // The activity tab provider.
    private ActivityTabProvider mTabProvider;

    private ButtonData mButtonData;
    private ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();
    private final ObservableSupplier<Boolean> mBottomToolbarVisibilitySupplier;
    private OnClickListener mOnClickListener;

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

        mButtonData = new ButtonData(false,
                AppCompatResources.getDrawable(mContext, R.drawable.ic_share_white_24dp),
                mOnClickListener, R.string.share, true, null);
    }

    @Override
    public void destroy() {}

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
        updateButtonState(tab);
        return mButtonData;
    }

    private void updateButtonState(Tab tab) {
        // TODO(crbug.com/1036023) add width constraints.
        if (!CachedFeatureFlags.isEnabled(ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR)
                || (mBottomToolbarVisibilitySupplier.get()
                        && BottomToolbarVariationManager.isShareButtonOnBottom())
                || mShareDelegateSupplier.get() == null) {
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
