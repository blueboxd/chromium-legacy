// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.scroll;

import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.components.feed.core.proto.libraries.sharedstream.ScrollStateProto.ScrollState;

/** Helper for restoring scroll positions. */
public class ScrollRestoreHelper {
    private static final String TAG = "ScrollUtils";
    private static final boolean ABANDON_RESTORE_BELOW_FOLD_DEFAULT = true;
    private static final long ABANDON_RESTORE_BELOW_FOLD_THRESHOLD_DEFAULT = 10L;

    /** Private constructor to prevent instantiation. */
    private ScrollRestoreHelper() {}

    /**
     * Returns a {@link ScrollState} for scroll position restore, or {@literal null} if the scroll
     * position can't or shouldn't be restored.
     *
     * @param currentHeaderCount The amount of headers which appear before Stream content.
     */
    /*@Nullable*/
    public static ScrollState getScrollStateForScrollRestore(LinearLayoutManager layoutManager,
            Configuration configuration, int currentHeaderCount) {
        int firstVisibleItemPosition = layoutManager.findFirstVisibleItemPosition();
        View firstVisibleView = layoutManager.findViewByPosition(firstVisibleItemPosition);
        int firstVisibleTop =
                firstVisibleView == null ? RecyclerView.NO_POSITION : firstVisibleView.getTop();
        int lastVisibleItemPosition = layoutManager.findLastVisibleItemPosition();

        return getScrollStateForScrollRestore(firstVisibleItemPosition, firstVisibleTop,
                lastVisibleItemPosition, configuration, currentHeaderCount);
    }

    /*@Nullable*/
    public static ScrollState getScrollStateForScrollRestore(int firstVisibleItemPosition,
            int firstVisibleTop, int lastVisibleItemPosition, Configuration configuration,
            int currentHeaderCount) {
        // If either of the firstVisibleItemPosition or firstVisibleTop are unknown, we shouldn't
        // restore scroll, so return null.
        if (firstVisibleItemPosition == RecyclerView.NO_POSITION
                || firstVisibleTop == RecyclerView.NO_POSITION) {
            return null;
        }

        // Determine if we can restore past the fold.
        if (configuration.getValueOrDefault(
                    ConfigKey.ABANDON_RESTORE_BELOW_FOLD, ABANDON_RESTORE_BELOW_FOLD_DEFAULT)) {
            long threshold =
                    configuration.getValueOrDefault(ConfigKey.ABANDON_RESTORE_BELOW_FOLD_THRESHOLD,
                            ABANDON_RESTORE_BELOW_FOLD_THRESHOLD_DEFAULT);

            if (lastVisibleItemPosition == RecyclerView.NO_POSITION
                    || lastVisibleItemPosition - currentHeaderCount > threshold) {
                Logger.w(TAG,
                        "Abandoning scroll due to fold threshold.  Bottom scroll index: %d, Header "
                                + "count: %d, Configured Threshold: %d",
                        lastVisibleItemPosition, currentHeaderCount, threshold);
                return null;
            }
        }

        return ScrollState.newBuilder()
                .setPosition(firstVisibleItemPosition)
                .setOffset(firstVisibleTop)
                .build();
    }
}
