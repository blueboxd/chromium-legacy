// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.support.v4.widget.NestedScrollView;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import com.google.android.libraries.feed.api.client.stream.Stream;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.feed.FeedProcessScopeFactory;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.feed.StreamLifecycleManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator to control the explore surface. */
class ExploreSurfaceCoordinator implements FeedSurfaceCoordinator.FeedSurfaceDelegate {
    private final ChromeActivity mActivity;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private final FeedSurfaceCreator mFeedSurfaceCreator;

    // mExploreSurfaceNavigationDelegate is lightweight, we keep it across FeedSurfaceCoordinators
    // after creating it during the first show.
    private ExploreSurfaceNavigationDelegate mExploreSurfaceNavigationDelegate;

    /** Interface to create {@link FeedSurfaceCoordinator} */
    interface FeedSurfaceCreator {
        /**
         * Creates the {@link FeedSurfaceCoordinator} for the specified mode.
         * @return The {@link FeedSurfaceCoordinator}.
         */
        FeedSurfaceCoordinator createFeedSurfaceCoordinator();
    }

    ExploreSurfaceCoordinator(ChromeActivity activity, ViewGroup parentView,
            @Nullable ViewGroup headerContainerView, PropertyModel containerPropertyModel) {
        mActivity = activity;

        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(containerPropertyModel,
                new ExploreSurfaceViewBinder.ViewHolder(parentView,
                        headerContainerView == null
                                ? null
                                : (NestedScrollView) LayoutInflater.from(activity).inflate(
                                        R.layout.ss_explore_scroll_container, parentView, false),
                        headerContainerView),
                ExploreSurfaceViewBinder::bind);
        mFeedSurfaceCreator = new FeedSurfaceCreator() {
            @Override
            public FeedSurfaceCoordinator createFeedSurfaceCoordinator() {
                return internalCreateFeedSurfaceCoordinator();
            }
        };
    }

    /**
     * Gets the {@link FeedSurfaceCreator}.
     * @return the {@link FeedSurfaceCreator}.
     */
    FeedSurfaceCreator getFeedSurfaceCreator() {
        return mFeedSurfaceCreator;
    }

    // Implements FeedSurfaceCoordinator.FeedSurfaceDelegate.
    @Override
    public StreamLifecycleManager createStreamLifecycleManager(Stream stream, Activity activity) {
        return new ExploreSurfaceStreamLifecycleManager(stream, activity);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return false;
    }

    private FeedSurfaceCoordinator internalCreateFeedSurfaceCoordinator() {
        if (mExploreSurfaceNavigationDelegate == null)
            mExploreSurfaceNavigationDelegate = new ExploreSurfaceNavigationDelegate(mActivity);

        ExploreSurfaceActionHandler exploreSurfaceActionHandler =
                new ExploreSurfaceActionHandler(mExploreSurfaceNavigationDelegate,
                        FeedProcessScopeFactory.getFeedConsumptionObserver(),
                        FeedProcessScopeFactory.getFeedOfflineIndicator(),
                        OfflinePageBridge.getForProfile(Profile.getLastUsedProfile()),
                        FeedProcessScopeFactory.getFeedLoggingBridge());
        return new FeedSurfaceCoordinator(
                mActivity, null, null, null, exploreSurfaceActionHandler, false, this);
        // TODO(crbug.com/982018): Customize surface background for incognito and dark mode.
        // TODO(crbug.com/982018): Hide signin promo UI in incognito mode.
    }
}
