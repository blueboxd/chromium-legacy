// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.modelprovider;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChange;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChangeObserver;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;

import java.util.ArrayList;
import java.util.HashSet;

/** Fake for {@link ModelFeature}. */
public class FakeModelFeature implements ModelFeature {
    private final StreamFeature mStreamFeature;
    private final ModelCursor mModelCursor;
    private final HashSet<FeatureChangeObserver> mObservers = new HashSet<>();

    private FakeModelFeature(ModelCursor modelCursor, StreamFeature streamFeature) {
        this.mModelCursor = modelCursor;
        this.mStreamFeature = streamFeature;
    }

    public static Builder newBuilder() {
        return new Builder();
    }

    public HashSet<FeatureChangeObserver> getObservers() {
        return mObservers;
    }

    public void triggerOnChange(FeatureChange change) {
        for (FeatureChangeObserver observer : mObservers) {
            observer.onChange(change);
        }
    }

    @Override
    public StreamFeature getStreamFeature() {
        return mStreamFeature;
    }

    @Override
    public ModelCursor getCursor() {
        return mModelCursor;
    }

    @Override
    public @Nullable ModelCursor getDirectionalCursor(
            boolean forward, @Nullable String startingChild) {
        return null;
    }

    @Override
    public void registerObserver(FeatureChangeObserver observer) {
        this.mObservers.add(observer);
    }

    @Override
    public void unregisterObserver(FeatureChangeObserver observer) {
        this.mObservers.remove(observer);
    }

    public static class Builder {
        private ModelCursor mModelCursor = new FakeModelCursor(new ArrayList<>());
        private StreamFeature mStreamFeature = StreamFeature.getDefaultInstance();

        private Builder() {}

        public Builder setModelCursor(ModelCursor modelCursor) {
            this.mModelCursor = modelCursor;
            return this;
        }

        public Builder setStreamFeature(StreamFeature streamFeature) {
            this.mStreamFeature = streamFeature;
            return this;
        }

        public FakeModelFeature build() {
            return new FakeModelFeature(mModelCursor, mStreamFeature);
        }
    }
}
