// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider.internal;

import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChangeObserver;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.common.feedobservable.FeedObservable;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;

import java.util.ArrayList;
import java.util.List;

/** Implementation of the {@link ModelFeature} */
public final class UpdatableModelFeature
        extends FeedObservable<FeatureChangeObserver> implements ModelFeature {
    private StreamFeature mStreamFeature;
    private final CursorProvider mCursorProvider;

    UpdatableModelFeature(StreamFeature streamFeature, CursorProvider cursorProvider) {
        this.mStreamFeature = streamFeature;
        this.mCursorProvider = cursorProvider;
    }

    @Override
    public StreamFeature getStreamFeature() {
        return mStreamFeature;
    }

    @Override
    public ModelCursor getCursor() {
        return mCursorProvider.getCursor(mStreamFeature.getContentId());
    }

    @Override
    /*@Nullable*/
    public ModelCursor getDirectionalCursor(boolean forward, /*@Nullable*/ String startingChild) {
        return null;
    }

    public List<FeatureChangeObserver> getObserversToNotify() {
        synchronized (mObservers) {
            return new ArrayList<>(mObservers);
        }
    }

    void setFeatureValue(StreamFeature streamFeature) {
        this.mStreamFeature = streamFeature;
    }
}
