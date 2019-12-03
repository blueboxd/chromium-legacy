// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import android.content.Context;
import android.content.res.Configuration;
import android.support.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.MediaQueryCondition;

import java.util.List;

/** Provides methods for filtering by MediaQueryConditions, based on a static status. */
public class MediaQueryHelper {
    private static final String TAG = "MediaQueryHelper";

    private final int mFrameWidthPx;
    private final int mDeviceOrientation;
    private final boolean mIsDarkTheme;
    private final Context mContext;

    MediaQueryHelper(int frameWidthPx, AssetProvider assetProvider, Context context) {
        this.mFrameWidthPx = frameWidthPx;
        this.mDeviceOrientation = context.getResources().getConfiguration().orientation;
        this.mIsDarkTheme = assetProvider.isDarkTheme();
        this.mContext = context;
    }

    @VisibleForTesting
    MediaQueryHelper(
            int frameWidthPx, int deviceOrientation, boolean isDarkTheme, Context context) {
        this.mFrameWidthPx = frameWidthPx;
        this.mDeviceOrientation = deviceOrientation;
        this.mIsDarkTheme = isDarkTheme;
        this.mContext = context;
    }

    boolean areMediaQueriesMet(List<MediaQueryCondition> conditions) {
        for (MediaQueryCondition condition : conditions) {
            if (!isMediaQueryMet(condition)) {
                return false;
            }
        }
        return true;
    }

    @SuppressWarnings("UnnecessaryDefaultInEnumSwitch")
    @VisibleForTesting
    boolean isMediaQueryMet(MediaQueryCondition condition) {
        switch (condition.getConditionCase()) {
            case FRAME_WIDTH:
                int targetWidth =
                        (int) LayoutUtils.dpToPx(condition.getFrameWidth().getWidth(), mContext);
                switch (condition.getFrameWidth().getCondition()) {
                    case EQUALS:
                        return mFrameWidthPx == targetWidth;
                    case GREATER_THAN:
                        return mFrameWidthPx > targetWidth;
                    case LESS_THAN:
                        return mFrameWidthPx < targetWidth;
                    case NOT_EQUALS:
                        return mFrameWidthPx != targetWidth;
                    default:
                }
                throw new PietFatalException(ErrorCode.ERR_INVALID_MEDIA_QUERY_CONDITION,
                        String.format("Unhandled ComparisonCondition: %s",
                                condition.getFrameWidth().getCondition().name()));
            case ORIENTATION:
                switch (condition.getOrientation().getOrientation()) {
                    case LANDSCAPE:
                        return mDeviceOrientation == Configuration.ORIENTATION_LANDSCAPE;
                    case UNSPECIFIED:
                        Logger.w(TAG, "Got UNSPECIFIED orientation; defaulting to PORTRAIT");
                        // fall through
                    case PORTRAIT:
                        return mDeviceOrientation == Configuration.ORIENTATION_PORTRAIT
                                || mDeviceOrientation == Configuration.ORIENTATION_SQUARE;
                }
                throw new PietFatalException(ErrorCode.ERR_INVALID_MEDIA_QUERY_CONDITION,
                        String.format("Unhandled Orientation: %s",
                                condition.getOrientation().getOrientation()));
            case DARK_LIGHT:
                switch (condition.getDarkLight().getMode()) {
                    case DARK:
                        return mIsDarkTheme;
                    case UNSPECIFIED:
                        Logger.w(TAG, "Got UNSPECIFIED DarkLightMode; defaulting to LIGHT");
                        // fall through
                    case LIGHT:
                        return !mIsDarkTheme;
                }
                throw new PietFatalException(ErrorCode.ERR_INVALID_MEDIA_QUERY_CONDITION,
                        String.format(
                                "Unhandled DarkLightMode: %s", condition.getDarkLight().getMode()));
            default:
                throw new PietFatalException(ErrorCode.ERR_INVALID_MEDIA_QUERY_CONDITION,
                        String.format(
                                "Unhandled MediaQueryCondition: %s", condition.getConditionCase()));
        }
    }

    @Override
    public boolean equals(/*@Nullable*/ Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof MediaQueryHelper)) {
            return false;
        }
        MediaQueryHelper that = (MediaQueryHelper) o;
        return mFrameWidthPx == that.mFrameWidthPx && mDeviceOrientation == that.mDeviceOrientation
                && mIsDarkTheme == that.mIsDarkTheme && mContext.equals(that.mContext);
    }

    @Override
    public int hashCode() {
        int result = mFrameWidthPx;
        result = 31 * result + mDeviceOrientation;
        result = 31 * result + (mIsDarkTheme ? 1 : 0);
        result = 31 * result + mContext.hashCode();
        return result;
    }
}
