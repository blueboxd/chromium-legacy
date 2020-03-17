// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.os.SystemClock;
import android.text.format.DateUtils;

import org.chromium.base.UserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.net.NetError;

/**
 * Centralizes UMA data collection for Tab management.
 * This will drive our memory optimization efforts, specially tab restoring and
 * eviction.
 * All calls must be made from the UI thread.
 */
public class TabUma extends EmptyTabObserver implements UserData {
    private static final Class<TabUma> USER_DATA_KEY = TabUma.class;

    // TabStatus defined in tools/metrics/histograms/histograms.xml.
    static final int TAB_STATUS_MEMORY_RESIDENT = 0;
    static final int TAB_STATUS_RELOAD_EVICTED = 1;
    static final int TAB_STATUS_RELOAD_COLD_START_FG = 6;
    static final int TAB_STATUS_RELOAD_COLD_START_BG = 7;
    static final int TAB_STATUS_LAZY_LOAD_FOR_BG_TAB = 8;
    static final int TAB_STATUS_LIM = 9;

    // The enum values for the Tab.RestoreResult histogram. The unusual order is to
    // keep compatibility with the previous instance of the histogram that was using
    // a boolean.
    //
    // Defined in tools/metrics/histograms/histograms.xml.
    private static final int TAB_RESTORE_RESULT_FAILURE_OTHER = 0;
    private static final int TAB_RESTORE_RESULT_SUCCESS = 1;
    private static final int TAB_RESTORE_RESULT_FAILURE_NETWORK_CONNECTIVITY = 2;
    private static final int TAB_RESTORE_RESULT_COUNT = 3;

    // TAB_STATE_* are for TabStateTransferTime and TabTransferTarget histograms.
    // TabState defined in tools/metrics/histograms/histograms.xml.
    private static final int TAB_STATE_INITIAL = 0;
    private static final int TAB_STATE_ACTIVE = 1;
    private static final int TAB_STATE_INACTIVE = 2;
    private static final int TAB_STATE_DETACHED = 3;
    private static final int TAB_STATE_CLOSED = 4;
    private static final int TAB_STATE_MAX = TAB_STATE_CLOSED;

    // Counter of tab shows (as per onShow()) for all tabs.
    private static long sAllTabsShowCount;

    private @TabCreationState int mTabCreationState;

    // Timestamp when this tab was last shown.
    private long mLastShownTimestamp = -1;

    // Timestamp of the beginning of the current tab restore.
    private long mRestoreStartedAtMillis = -1;

    private long mLastTabStateChangeMillis = -1;
    private int mLastTabState = TAB_STATE_INITIAL;

    // The number of background tabs opened by long pressing on this tab and selecting
    // "Open in a new tab" from the context menu.
    private int mNumBackgroundTabsOpened;

    private TabModelSelectorObserver mNewTabObserver;

    /**
     * Creates {@link TabUma} instance optionally. Creates one only when tab creation type
     * is non-null.
     */
    static void createForTab(Tab tab) {
        assert tab.getUserDataHost().getUserData(USER_DATA_KEY) == null;
        @TabCreationState
        Integer creationState = ((TabImpl) tab).getCreationState();
        if (creationState != null) {
            tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabUma(tab, creationState));
        }
    }

    /**
     * Constructs a new UMA tracker for a specific tab.
     * @param tab Tab this UMA tracker is created for.
     * @param creationState In what state the tab was created.
     */
    private TabUma(Tab tab, @TabCreationState int creationState) {
        mLastTabStateChangeMillis = System.currentTimeMillis();
        mTabCreationState = creationState;
        switch (mTabCreationState) {
            case TabCreationState.LIVE_IN_FOREGROUND:
                updateTabState(TAB_STATE_ACTIVE);
                break;
            case TabCreationState.LIVE_IN_BACKGROUND: // Fall through
            case TabCreationState.FROZEN_ON_RESTORE: // Fall through
            case TabCreationState.FROZEN_FOR_LAZY_LOAD:
                updateTabState(TAB_STATE_INACTIVE);
        }
        tab.addObserver(this);
    }

    /**
     * Records the tab restore result into several UMA histograms.
     * @param succeeded Whether or not the tab restore succeeded.
     * @param time The time taken to perform the tab restore.
     * @param perceivedTime The perceived time taken to perform the tab restore.
     * @param errorCode The error code, on failure (as denoted by the |succeeded| parameter).
     */
    private void recordTabRestoreResult(
            boolean succeeded, long time, long perceivedTime, @NetError int errorCode) {
        if (succeeded) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Tab.RestoreResult", TAB_RESTORE_RESULT_SUCCESS, TAB_RESTORE_RESULT_COUNT);
            RecordHistogram.recordCountHistogram("Tab.RestoreTime", (int) time);
            RecordHistogram.recordCountHistogram("Tab.PerceivedRestoreTime", (int) perceivedTime);
        } else {
            switch (errorCode) {
                case NetError.ERR_INTERNET_DISCONNECTED:
                case NetError.ERR_NAME_RESOLUTION_FAILED:
                case NetError.ERR_DNS_TIMED_OUT:
                    RecordHistogram.recordEnumeratedHistogram("Tab.RestoreResult",
                            TAB_RESTORE_RESULT_FAILURE_NETWORK_CONNECTIVITY,
                            TAB_RESTORE_RESULT_COUNT);
                    break;
                default:
                    RecordHistogram.recordEnumeratedHistogram("Tab.RestoreResult",
                            TAB_RESTORE_RESULT_FAILURE_OTHER, TAB_RESTORE_RESULT_COUNT);
            }
        }
    }

    /**
     * Record the tab state transition into histograms.
     * @param prevState Previous state of the tab.
     * @param newState New state of the tab.
     * @param delta Time elapsed from the last state transition in milliseconds.
     */
    private void recordTabStateTransition(int prevState, int newState, long delta) {
        if (prevState == TAB_STATE_INITIAL) {
            RecordHistogram.recordEnumeratedHistogram("Tabs.StateTransfer.Target_Initial", newState,
                    TAB_STATE_MAX);
        } else if (prevState == TAB_STATE_ACTIVE) {
            RecordHistogram.recordEnumeratedHistogram("Tabs.StateTransfer.Target_Active", newState,
                    TAB_STATE_MAX);
        } else if (prevState == TAB_STATE_INACTIVE) {
            RecordHistogram.recordEnumeratedHistogram("Tabs.StateTransfer.Target_Inactive",
                    newState, TAB_STATE_MAX);
        }
    }

    /**
     * Records the number of background tabs which were opened from this tab's
     * current URL. Does not record anything if no background tabs were opened.
     */
    private void recordNumBackgroundTabsOpened() {
        if (mNumBackgroundTabsOpened > 0) {
            RecordHistogram.recordCount100Histogram(
                    "Tab.BackgroundTabsOpenedViaContextMenuCount", mNumBackgroundTabsOpened);
        }
        mNumBackgroundTabsOpened = 0;
    }

    /**
     * Updates saved TabState and its timestamp. Records the state transition into the histogram.
     * @param newState New state of the tab.
     */
    private void updateTabState(int newState) {
        if (mLastTabState == newState) {
            return;
        }
        long now = System.currentTimeMillis();
        recordTabStateTransition(mLastTabState, newState, now - mLastTabStateChangeMillis);
        mLastTabStateChangeMillis = now;
        mLastTabState = newState;
    }

    // TabObserver

    @Override
    public void onShown(Tab tab, @TabSelectionType int selectionType) {
        long previousTimestampMillis = tab.getTimestampMillis();
        long now = SystemClock.elapsedRealtime();

        // Do not collect the tab switching data for the first switch to a tab after the cold start
        // and for the tab switches that were not user-originated (e.g. the user closes the last
        // incognito tab and the current normal mode tab is shown).
        if (mLastShownTimestamp != -1 && selectionType == TabSelectionType.FROM_USER) {
            long age = now - mLastShownTimestamp;
            RecordHistogram.recordCountHistogram("Tab.SwitchedToForegroundAge", (int) age);
        }

        increaseTabShowCount();
        boolean isOnBrowserStartup = sAllTabsShowCount == 1;
        boolean performsLazyLoad = mTabCreationState == TabCreationState.FROZEN_FOR_LAZY_LOAD
                && mLastShownTimestamp == -1;

        int status;
        if (mRestoreStartedAtMillis == -1 && !performsLazyLoad) {
            // The tab is *not* being restored or loaded lazily on first display.
            status = TAB_STATUS_MEMORY_RESIDENT;
        } else if (mLastShownTimestamp == -1) {
            // This is first display and the tab is being restored or loaded lazily.
            if (isOnBrowserStartup) {
                status = TAB_STATUS_RELOAD_COLD_START_FG;
            } else if (mTabCreationState == TabCreationState.FROZEN_ON_RESTORE) {
                status = TAB_STATUS_RELOAD_COLD_START_BG;
            } else if (mTabCreationState == TabCreationState.FROZEN_FOR_LAZY_LOAD) {
                status = TAB_STATUS_LAZY_LOAD_FOR_BG_TAB;
            } else {
                assert mTabCreationState == TabCreationState.LIVE_IN_FOREGROUND
                        || mTabCreationState == TabCreationState.LIVE_IN_BACKGROUND;
                status = TAB_STATUS_RELOAD_EVICTED;
            }
        } else {
            // The tab is being restored and this is *not* the first time the tab is shown.
            status = TAB_STATUS_RELOAD_EVICTED;
        }

        // Record only user-visible switches to existing tabs. Do not record displays of newly
        // created tabs (FROM_NEW) or selections of the previous tab that happen when we close the
        // tab opened from intent while exiting Chrome (FROM_CLOSE).
        if (selectionType == TabSelectionType.FROM_USER) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Tab.StatusWhenSwitchedBackToForeground", status, TAB_STATUS_LIM);
        }

        if (mLastShownTimestamp == -1) {
            // Register the observer for context menu-triggering event here to avoid the case
            // where this is created too early and we start missing out on metrics suddenly.
            mNewTabObserver = new EmptyTabModelSelectorObserver() {
                @Override
                public void onNewTabCreated(Tab newTab, @TabCreationState int creationState) {
                    if (newTab.getParentId() == tab.getId()
                            && newTab.getLaunchType() == TabLaunchType.FROM_LONGPRESS_BACKGROUND) {
                        ++mNumBackgroundTabsOpened;
                    }
                }
            };
            TabModelSelector.from(tab).addObserver(mNewTabObserver);
        }

        // Record "tab age upon first display" metrics. previousTimestampMillis is persisted through
        // cold starts.
        if (mLastShownTimestamp == -1 && previousTimestampMillis > 0) {
            long duration = System.currentTimeMillis() - previousTimestampMillis;
            if (mTabCreationState == TabCreationState.LIVE_IN_BACKGROUND
                    && mRestoreStartedAtMillis != -1) {
                RecordHistogram.recordMediumTimesHistogram(
                        "Tab.LostTabAgeWhenSwitchedToForeground", duration);
            }
            if (isOnBrowserStartup) {
                RecordHistogram.recordCountHistogram("Tabs.ForegroundTabAgeAtStartup",
                        (int) (duration / DateUtils.MINUTE_IN_MILLIS));
            } else if (selectionType == TabSelectionType.FROM_USER) {
                RecordHistogram.recordCountHistogram("Tab.AgeUponRestoreFromColdStart",
                        (int) (duration / DateUtils.MINUTE_IN_MILLIS));
            }
        }

        mLastShownTimestamp = now;

        updateTabState(TAB_STATE_ACTIVE);
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        if (type == TabHidingType.ACTIVITY_HIDDEN) {
            recordNumBackgroundTabsOpened();
        } else {
            updateTabState(TAB_STATE_INACTIVE);
        }
    }

    @Override
    public void onDestroyed(Tab tab) {
        updateTabState(TAB_STATE_CLOSED);

        if (mTabCreationState == TabCreationState.LIVE_IN_BACKGROUND
                || mTabCreationState == TabCreationState.FROZEN_FOR_LAZY_LOAD) {
            RecordHistogram.recordBooleanHistogram(
                    "Tab.BackgroundTabShown", mLastShownTimestamp != -1);
        }

        recordNumBackgroundTabsOpened();
        if (mNewTabObserver != null) {
            TabModelSelector.from(tab).removeObserver(mNewTabObserver);
            mNewTabObserver = null;
        }
        tab.removeObserver(this);
    }

    @Override
    public void onRestoreStarted(Tab tab) {
        mRestoreStartedAtMillis = SystemClock.elapsedRealtime();
    }

    @Override
    public void onRestoreFailed(Tab tab) {
        assert mRestoreStartedAtMillis == -1;
        if (mLastTabState == TAB_STATE_ACTIVE) {
            mTabCreationState = TabCreationState.LIVE_IN_FOREGROUND;
        } else {
            mTabCreationState = TabCreationState.LIVE_IN_BACKGROUND;
        }
    }

    /** Called when the corresponding tab starts a page load. */
    @Override
    public void onPageLoadStarted(Tab tab, String url) {
        recordNumBackgroundTabsOpened();
    }

    /** Called when the corresponding tab completes a page load. */
    @Override
    public void onPageLoadFinished(Tab tab, String url) {
        // Record only tab restores that the user became aware of. If the restore is triggered
        // speculatively and completes before the user switches to the tab, then this case is
        // reflected in Tab.StatusWhenSwitchedBackToForeground metric.
        if (mRestoreStartedAtMillis != -1 && mLastShownTimestamp >= mRestoreStartedAtMillis) {
            long now = SystemClock.elapsedRealtime();
            long restoreTime = now - mRestoreStartedAtMillis;
            long perceivedRestoreTime = now - mLastShownTimestamp;
            recordTabRestoreResult(true, restoreTime, perceivedRestoreTime, -1);
        }
        mRestoreStartedAtMillis = -1;
    }

    /** Called when the corresponding tab fails a page load. */
    @Override
    public void onPageLoadFailed(Tab tab, int errorCode) {
        if (mRestoreStartedAtMillis != -1 && mLastShownTimestamp >= mRestoreStartedAtMillis) {
            // Load time is ignored for failed loads.
            recordTabRestoreResult(false, -1, -1, errorCode);
        }
        mRestoreStartedAtMillis = -1;
    }

    /** Called when the renderer of the corresponding tab crashes. */
    @Override
    public void onCrash(Tab tab) {
        if (mRestoreStartedAtMillis != -1) {
            // TODO(ppi): Add a bucket in Tab.RestoreResult for restores failed due to
            //            renderer crashes and start to track that.
            mRestoreStartedAtMillis = -1;
        }
    }

    private static void increaseTabShowCount() {
        sAllTabsShowCount++;
    }
}
