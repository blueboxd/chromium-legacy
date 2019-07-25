// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.content.ComponentName;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.Build;
import android.support.annotation.Nullable;
import android.util.Pair;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View;
import android.view.View.OnCreateContextMenuListener;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.MenuSourceType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.OnCloseContextMenuListener;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * A helper class that handles generating context menus for {@link WebContents}s.
 */
public class ContextMenuHelper implements OnCreateContextMenuListener {
    public static Callback<RevampedContextMenuCoordinator> sRevampedContextMenuShownCallback;

    private static final int MAX_SHARE_DIMEN_PX = 2048;

    private final WebContents mWebContents;
    private long mNativeContextMenuHelper;

    private ContextMenuPopulator mPopulator;
    private ContextMenuParams mCurrentContextMenuParams;
    private Activity mActivity;
    private Callback<Integer> mCallback;
    private Runnable mOnMenuShown;
    private Callback<Boolean> mOnMenuClosed;
    private long mMenuShownTimeMs;
    private boolean mSelectedItemBeforeDismiss;

    private ContextMenuHelper(long nativeContextMenuHelper, WebContents webContents) {
        mNativeContextMenuHelper = nativeContextMenuHelper;
        mWebContents = webContents;
    }

    @CalledByNative
    private static ContextMenuHelper create(long nativeContextMenuHelper, WebContents webContents) {
        return new ContextMenuHelper(nativeContextMenuHelper, webContents);
    }

    @CalledByNative
    private void destroy() {
        if (mPopulator != null) mPopulator.onDestroy();
        mNativeContextMenuHelper = 0;
    }

    /**
     * @return The activity that corresponds to the context menu helper.
     */
    protected Activity getActivity() {
        return mActivity;
    }

    /**
     * @param populator A {@link ContextMenuPopulator} that is responsible for managing and showing
     *                  context menus.
     */
    @CalledByNative
    private void setPopulator(ContextMenuPopulator populator) {
        if (mPopulator != null) mPopulator.onDestroy();
        mPopulator = populator;
    }

    /**
     * Starts showing a context menu for {@code view} based on {@code params}.
     * @param params The {@link ContextMenuParams} that indicate what menu items to show.
     * @param view container view for the menu.
     * @param topContentOffsetPx the offset of the content from the top.
     */
    @CalledByNative
    private void showContextMenu(
            final ContextMenuParams params, View view, float topContentOffsetPx) {
        if (params.isFile()) return;
        final WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();

        if (view == null || view.getVisibility() != View.VISIBLE || view.getParent() == null
                || windowAndroid == null || windowAndroid.getActivity().get() == null
                || mPopulator == null) {
            return;
        }

        mCurrentContextMenuParams = params;
        mActivity = windowAndroid.getActivity().get();
        mCallback = (result) -> {
            mSelectedItemBeforeDismiss = true;
            mPopulator.onItemSelected(ContextMenuHelper.this, mCurrentContextMenuParams, result);
        };
        mOnMenuShown = () -> {
            mSelectedItemBeforeDismiss = false;
            mMenuShownTimeMs = TimeUnit.MICROSECONDS.toMillis(TimeUtils.nativeGetTimeTicksNowUs());
            RecordHistogram.recordBooleanHistogram("ContextMenu.Shown", mWebContents != null);
        };
        mOnMenuClosed = (notAbandoned) -> {
            recordTimeToTakeActionHistogram(mSelectedItemBeforeDismiss || notAbandoned);
            if (mNativeContextMenuHelper == 0) return;
            nativeOnContextMenuClosed(mNativeContextMenuHelper);
        };

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.REVAMPED_CONTEXT_MENU)
                && params.getSourceType() != MenuSourceType.MENU_SOURCE_MOUSE) {
            List<Pair<Integer, List<ContextMenuItem>>> items =
                    mPopulator.buildContextMenu(null, mActivity, mCurrentContextMenuParams);
            if (items.isEmpty()) {
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> mOnMenuClosed.onResult(false));
                return;
            }

            final RevampedContextMenuCoordinator menuCoordinator =
                    new RevampedContextMenuCoordinator(
                            topContentOffsetPx, this::shareImageWithLastShareComponent);
            menuCoordinator.displayMenu(mActivity, mCurrentContextMenuParams, items, mCallback,
                    mOnMenuShown, mOnMenuClosed);
            if (sRevampedContextMenuShownCallback != null) {
                sRevampedContextMenuShownCallback.onResult(menuCoordinator);
            }
            // TODO(sinansahin): This could be pushed in to the header mediator.
            if (mCurrentContextMenuParams.isImage()) {
                getThumbnail(menuCoordinator.getOnImageThumbnailRetrievedReference());
            }
            return;
        }

        // The Platform Context Menu requires the listener within this helper since this helper and
        // provides context menu for us to show.
        view.setOnCreateContextMenuListener(this);
        boolean wasContextMenuShown = false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                && params.getSourceType() == MenuSourceType.MENU_SOURCE_MOUSE) {
            final float density = view.getResources().getDisplayMetrics().density;
            final float touchPointXPx = params.getTriggeringTouchXDp() * density;
            final float touchPointYPx =
                    (params.getTriggeringTouchYDp() * density) + topContentOffsetPx;
            wasContextMenuShown = view.showContextMenu(touchPointXPx, touchPointYPx);
        } else {
            wasContextMenuShown = view.showContextMenu();
        }
        if (wasContextMenuShown) {
            mOnMenuShown.run();
            windowAndroid.addContextMenuCloseListener(new OnCloseContextMenuListener() {
                @Override
                public void onContextMenuClosed() {
                    mOnMenuClosed.onResult(false);
                    windowAndroid.removeContextMenuCloseListener(this);
                }
            });
        }
    }

    /**
     * Starts a download based on the current {@link ContextMenuParams}.
     * @param isLink Whether or not the download target is a link.
     */
    public void startContextMenuDownload(boolean isLink, boolean isDataReductionProxyEnabled) {
        if (mNativeContextMenuHelper != 0) {
            nativeOnStartDownload(mNativeContextMenuHelper, isLink, isDataReductionProxyEnabled);
        }
    }

    /**
     * Trigger an image search for the current image that triggered the context menu.
     */
    public void searchForImage() {
        if (mNativeContextMenuHelper == 0) return;
        nativeSearchForImage(mNativeContextMenuHelper);
    }

    /**
     * Share the image that triggered the current context menu.
     * Package-private, allowing access only from the context menu item to ensure that
     * it will use the right activity set when the menu was displayed.
     */
    void shareImage() {
        shareImageDirectly(null);
    }

    /**
     * Share the image that triggered the current context menu with the last app used to share.
     */
    private void shareImageWithLastShareComponent() {
        shareImageDirectly(ShareHelper.getLastShareComponentName(null));
    }

    /**
     * Share image triggered with the current context menu directly with a specific app.
     * @param name The {@link ComponentName} of the app to share the image directly with.
     */
    private void shareImageDirectly(@Nullable final ComponentName name) {
        if (mNativeContextMenuHelper == 0) return;
        Callback<byte[]> callback = new Callback<byte[]>() {
            @Override
            public void onResult(byte[] result) {
                if (mActivity == null) return;

                ShareHelper.shareImage(mActivity, result, name);
            }
        };
        nativeRetrieveImageForShare(
                mNativeContextMenuHelper, callback, MAX_SHARE_DIMEN_PX, MAX_SHARE_DIMEN_PX);
    }

    /**
     * Gets the thumbnail of the current image that triggered the context menu.
     * @param callback Called once the the thumbnail is received.
     */
    private void getThumbnail(final Callback<Bitmap> callback) {
        if (mNativeContextMenuHelper == 0) return;

        final Resources res = mActivity.getResources();
        final int maxHeightPx =
                res.getDimensionPixelSize(R.dimen.revamped_context_menu_header_image_max_size);
        final int maxWidthPx =
                res.getDimensionPixelSize(R.dimen.revamped_context_menu_header_image_max_size);

        nativeRetrieveImageForContextMenu(
                mNativeContextMenuHelper, callback, maxWidthPx, maxHeightPx);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        List<Pair<Integer, List<ContextMenuItem>>> items =
                mPopulator.buildContextMenu(menu, v.getContext(), mCurrentContextMenuParams);

        if (items.isEmpty()) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> mOnMenuClosed.onResult(false));
            return;
        }
        ContextMenuUi menuUi = new PlatformContextMenuUi(menu);
        menuUi.displayMenu(mActivity, mCurrentContextMenuParams, items, mCallback, mOnMenuShown,
                mOnMenuClosed);
    }

    private void recordTimeToTakeActionHistogram(boolean selectedItem) {
        final String action = selectedItem ? "SelectedItem" : "Abandoned";
        RecordHistogram.recordTimesHistogram("ContextMenu.TimeToTakeAction." + action,
                TimeUnit.MICROSECONDS.toMillis(TimeUtils.nativeGetTimeTicksNowUs())
                        - mMenuShownTimeMs);
    }

    /**
     * @return The {@link ContextMenuPopulator} responsible for populating the context menu.
     */
    @VisibleForTesting
    public ContextMenuPopulator getPopulator() {
        return mPopulator;
    }

    private native void nativeOnStartDownload(
            long nativeContextMenuHelper, boolean isLink, boolean isDataReductionProxyEnabled);
    private native void nativeSearchForImage(long nativeContextMenuHelper);
    private native void nativeRetrieveImageForShare(long nativeContextMenuHelper,
            Callback<byte[]> callback, int maxWidthPx, int maxHeightPx);
    private native void nativeRetrieveImageForContextMenu(long nativeContextMenuHelper,
            Callback<Bitmap> callback, int maxWidthPx, int maxHeightPx);
    private native void nativeOnContextMenuClosed(long nativeContextMenuHelper);
}
