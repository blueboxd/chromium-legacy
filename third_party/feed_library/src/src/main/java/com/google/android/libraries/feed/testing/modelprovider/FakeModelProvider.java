// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.modelprovider;

import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelMutation;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderObserver;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelToken;
import com.google.common.collect.ImmutableList;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Fake for tests using {@link ModelProvider}. Functionality should be added to this class as
 * needed.
 */
public class FakeModelProvider implements ModelProvider {
    private final Set<ModelProviderObserver> mObservers = new HashSet<>();
    private ModelFeature mRootFeature;
    private boolean mWasRefreshTriggered;
    private boolean mTokensEnabled = true;
    /*@Nullable*/ private String mSessionId;
    @RequestReason
    private int mLastRequestReason = RequestReason.UNKNOWN;
    /*@Nullable*/ private ModelFeature mImmediateSessionStartModel;
    /*@Nullable*/ private UiContext mUnusedTriggerRefreshUiContext;
    private FakeModelMutation mLatestModelMutation = new FakeModelMutation();
    private boolean mIsInvalidated;

    @Override
    public ModelMutation edit() {
        mLatestModelMutation = new FakeModelMutation();
        return mLatestModelMutation;
    }

    @Override
    public void invalidate() {
        mIsInvalidated = true;
    }

    @Override
    public void invalidate(UiContext uiContext) {
        mIsInvalidated = true;
    }

    @Override
    public void detachModelProvider() {}

    @Override
    public void raiseError(ModelError error) {}

    @Override
    /*@Nullable*/
    public ModelFeature getRootFeature() {
        return mRootFeature;
    }

    @Override
    /*@Nullable*/
    public ModelChild getModelChild(String contentId) {
        return null;
    }

    @Override
    /*@Nullable*/
    public StreamSharedState getSharedState(ContentId contentId) {
        return null;
    }

    @Override
    public boolean handleToken(ModelToken modelToken) {
        return mTokensEnabled;
    }

    @Override
    public void triggerRefresh(@RequestReason int requestReason) {
        triggerRefresh(requestReason, UiContext.getDefaultInstance());
    }

    @Override
    public void triggerRefresh(@RequestReason int requestReason, UiContext uiContext) {
        mWasRefreshTriggered = true;
        mLastRequestReason = requestReason;
        mUnusedTriggerRefreshUiContext = uiContext;
    }

    @Override
    public int getCurrentState() {
        return State.INITIALIZING;
    }

    @Override
    /*@Nullable*/
    public String getSessionId() {
        return mSessionId;
    }

    @Override
    public List<ModelChild> getAllRootChildren() {
        ImmutableList.Builder<ModelChild> listBuilder = ImmutableList.builder();
        if (mRootFeature == null) {
            return listBuilder.build();
        }

        ModelCursor cursor = mRootFeature.getCursor();
        ModelChild child;
        while ((child = cursor.getNextItem()) != null) {
            listBuilder.add(child);
        }
        return listBuilder.build();
    }

    @Override
    public void enableRemoveTracking(RemoveTrackingFactory<?> removeTrackingFactory) {}

    @Override
    public void registerObserver(ModelProviderObserver observer) {
        if (mImmediateSessionStartModel != null) {
            this.mRootFeature = mImmediateSessionStartModel;
            observer.onSessionStart(UiContext.getDefaultInstance());
        }

        mObservers.add(observer);
    }

    @Override
    public void unregisterObserver(ModelProviderObserver observer) {
        mObservers.remove(observer);
    }

    public void triggerOnSessionStart(ModelFeature rootFeature) {
        triggerOnSessionStart(rootFeature, UiContext.getDefaultInstance());
    }

    public void triggerOnSessionStart(ModelFeature rootFeature, UiContext uiContext) {
        this.mRootFeature = rootFeature;
        for (ModelProviderObserver observer : mObservers) {
            observer.onSessionStart(uiContext);
        }
    }

    public void triggerOnSessionFinished() {
        triggerOnSessionFinished(UiContext.getDefaultInstance());
    }

    public void triggerOnSessionFinished(UiContext uiContext) {
        for (ModelProviderObserver observer : mObservers) {
            observer.onSessionFinished(uiContext);
        }
    }

    public void triggerOnError(ModelError modelError) {
        for (ModelProviderObserver observer : mObservers) {
            observer.onError(modelError);
        }
    }

    public boolean wasRefreshTriggered() {
        return mWasRefreshTriggered;
    }

    public Set<ModelProviderObserver> getObservers() {
        return mObservers;
    }

    public void setSessionId(String sessionId) {
        this.mSessionId = sessionId;
    }

    @RequestReason
    public int getLastRequestReason() {
        return mLastRequestReason;
    }

    public void triggerOnSessionStartImmediately(ModelFeature modelFeature) {
        mImmediateSessionStartModel = modelFeature;
    }

    public void setTokensEnabled(boolean value) {
        mTokensEnabled = value;
    }

    /** Returns the last {@link FakeModelMutation} returned from the {@link edit()} method. */
    public FakeModelMutation getLatestModelMutation() {
        return mLatestModelMutation;
    }

    /** Returns whether this {@link ModelProvider} has been invalidated. */
    public boolean isInvalidated() {
        return mIsInvalidated;
    }
}
