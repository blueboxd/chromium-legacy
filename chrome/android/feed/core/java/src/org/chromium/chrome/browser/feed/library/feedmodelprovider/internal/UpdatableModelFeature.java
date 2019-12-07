// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider.internal;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChangeObserver;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.common.feedobservable.FeedObservable;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;

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
    @Nullable
    public ModelCursor getDirectionalCursor(boolean forward, @Nullable String startingChild) {
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
