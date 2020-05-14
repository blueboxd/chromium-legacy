// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ScrollListener;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ScrollListener.ScrollState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerState;

/**
 * Creates a ScrollListener that triggers the menu IPH. The listener removes itself from the
 * list of observers when the IPH is determined to be already triggered.
 *
 * Triggering the IPH is based on (1) the fraction of scroll done on the stream proportionally
 * to its height, (2) the transition fraction of the top search bar, and (3) the position of the
 * menu button in the stream.
 *
 * We want the IPH to be triggered when the section header is properly positioned in the stream
 * which has to meet the following conditions: (1) the IPH popup won't interfere with the search
 * bar at the top of the NTP, (2) the user has scrolled down a bit because they want to look at
 * the feed, and (3) the feed header with its menu button is high enough in the stream to have
 * the feed visible. The goal of conditions (2) and (3) is to show the IPH when the signals are
 * that the user wants to interact with the feed are strong.
 */
class HeaderIphScrollListener implements ScrollListener {
    static final String TOOLBAR_TRANSITION_FRACTION_PARAM_NAME = "toolbar-transition-fraction";
    static final String MIN_SCROLL_FRACTION_PARAM_NAME = "min-scroll-fraction";
    static final String HEADER_MAX_POSITION_FRACTION_NAME = "header-max-pos-fraction";

    static interface Delegate {
        Tracker getFeatureEngagementTracker();
        Stream getStream();
        boolean isFeedHeaderPositionInRecyclerViewSuitableForIPH(float headerMaxPosFraction);
        void showMenuIph();
        int getVerticalScrollOffset();
        boolean isFeedExpanded();
        int getRootViewHeight();
        boolean isSignedIn();
    }

    private Delegate mDelegate;

    private float mMinScrollFraction;
    private float mHeaderMaxPosFraction;

    HeaderIphScrollListener(Delegate delegate) {
        mDelegate = delegate;

        mMinScrollFraction = (float) ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                ChromeFeatureList.REPORT_FEED_USER_ACTIONS, MIN_SCROLL_FRACTION_PARAM_NAME, 0.10);
        mHeaderMaxPosFraction = (float) ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                ChromeFeatureList.REPORT_FEED_USER_ACTIONS, HEADER_MAX_POSITION_FRACTION_NAME,
                0.35);
    }

    @Override
    public void onScrollStateChanged(@ScrollState int state) {
        // Get the feature tracker for the IPH and determine whether to show the IPH.
        final String featureForIph = FeatureConstants.FEED_HEADER_MENU_FEATURE;
        final Tracker tracker = mDelegate.getFeatureEngagementTracker();
        // Stop listening to scroll if the IPH was already displayed in the past.
        if (tracker.getTriggerState(featureForIph) == TriggerState.HAS_BEEN_DISPLAYED) {
            mDelegate.getStream().removeScrollListener(this);
            return;
        }

        // Don't do any calculation if the IPH cannot be triggered to avoid wasting efforts.
        if (!tracker.wouldTriggerHelpUI(featureForIph)) {
            return;
        }

        if (state != ScrollState.IDLE) return;

        // Check whether the feed is expanded.
        if (!mDelegate.isFeedExpanded()) return;

        // Check whether the user is signed in.
        if (!mDelegate.isSignedIn()) return;

        // Check that enough scrolling was done proportionally to the stream height.
        if ((float) mDelegate.getVerticalScrollOffset()
                < (float) mDelegate.getRootViewHeight() * mMinScrollFraction) {
            return;
        }

        // Check that the feed header is well positioned in the recycler view to show the IPH.
        if (!mDelegate.isFeedHeaderPositionInRecyclerViewSuitableForIPH(mHeaderMaxPosFraction)) {
            return;
        }

        mDelegate.showMenuIph();
    }

    @Override
    public void onScrolled(int dx, int dy) {}
}