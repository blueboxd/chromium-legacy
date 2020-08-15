// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

import java.util.HashMap;
import java.util.Map;

/** Helper class for recording metrics related to TabbedPaintPreview. */
public class TabbedPaintPreviewMetricsHelper {
    /** Used for recording the cause for exiting the Paint Preview player. */
    @IntDef({ExitCause.PULL_TO_REFRESH, ExitCause.SNACK_BAR_ACTION, ExitCause.COMPOSITOR_FAILURE,
            ExitCause.TAB_FINISHED_LOADING, ExitCause.LINK_CLICKED, ExitCause.NAVIGATION_STARTED,
            ExitCause.TAB_DESTROYED})
    @interface ExitCause {
        int PULL_TO_REFRESH = 0;
        int SNACK_BAR_ACTION = 1;
        int COMPOSITOR_FAILURE = 2;
        int TAB_FINISHED_LOADING = 3;
        int LINK_CLICKED = 4;
        int NAVIGATION_STARTED = 5;
        int TAB_DESTROYED = 6;
        int COUNT = 7;
    }

    private static final Map<Integer, String> UPTIME_HISTOGRAM_MAP = new HashMap<>();
    static {
        UPTIME_HISTOGRAM_MAP.put(ExitCause.PULL_TO_REFRESH,
                "Browser.PaintPreview.TabbedPlayer.UpTime.RemovedByPullToRefresh");
        UPTIME_HISTOGRAM_MAP.put(ExitCause.SNACK_BAR_ACTION,
                "Browser.PaintPreview.TabbedPlayer.UpTime.RemovedBySnackBar");
        UPTIME_HISTOGRAM_MAP.put(ExitCause.COMPOSITOR_FAILURE,
                "Browser.PaintPreview.TabbedPlayer.UpTime.RemovedByCompositorFailure");
        UPTIME_HISTOGRAM_MAP.put(ExitCause.TAB_FINISHED_LOADING,
                "Browser.PaintPreview.TabbedPlayer.UpTime.RemovedOnLoad");
        UPTIME_HISTOGRAM_MAP.put(ExitCause.LINK_CLICKED,
                "Browser.PaintPreview.TabbedPlayer.UpTime.RemovedByLinkClick");
        UPTIME_HISTOGRAM_MAP.put(ExitCause.NAVIGATION_STARTED,
                "Browser.PaintPreview.TabbedPlayer.UpTime.RemovedByNavigation");
        UPTIME_HISTOGRAM_MAP.put(ExitCause.TAB_DESTROYED,
                "Browser.PaintPreview.TabbedPlayer.UpTime.RemovedOnTabDestroy");
    }

    private long mShownTime;

    void onShown() {
        mShownTime = System.currentTimeMillis();
    }

    void recordExitMetrics(int exitCause, int snackbarShownCount) {
        if (exitCause == ExitCause.SNACK_BAR_ACTION) {
            RecordUserAction.record("PaintPreview.TabbedPlayer.Actionbar.Action");
        }

        RecordUserAction.record("PaintPreview.TabbedPlayer.Removed");
        RecordHistogram.recordCountHistogram(
                "Browser.PaintPreview.TabbedPlayer.SnackbarCount", snackbarShownCount);
        RecordHistogram.recordEnumeratedHistogram(
                "Browser.PaintPreview.TabbedPlayer.ExitCause", exitCause, ExitCause.COUNT);
        long upTime = System.currentTimeMillis() - mShownTime;
        if (UPTIME_HISTOGRAM_MAP.containsKey(exitCause)) {
            RecordHistogram.recordLongTimesHistogram(UPTIME_HISTOGRAM_MAP.get(exitCause), upTime);
        }
    }
}
