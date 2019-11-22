// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.modelprovider;

import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChange;
import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChangeObserver;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;

import java.util.ArrayList;
import java.util.HashSet;

/** Fake for {@link ModelFeature}. */
public class FakeModelFeature implements ModelFeature {
    private final StreamFeature streamFeature;
    private final ModelCursor modelCursor;
    private final HashSet<FeatureChangeObserver> observers = new HashSet<>();

    private FakeModelFeature(ModelCursor modelCursor, StreamFeature streamFeature) {
        this.modelCursor = modelCursor;
        this.streamFeature = streamFeature;
    }

    public static Builder newBuilder() {
        return new Builder();
    }

    public HashSet<FeatureChangeObserver> getObservers() {
        return observers;
    }

    public void triggerOnChange(FeatureChange change) {
        for (FeatureChangeObserver observer : observers) {
            observer.onChange(change);
        }
    }

    @Override
    public StreamFeature getStreamFeature() {
        return streamFeature;
    }

    @Override
    public ModelCursor getCursor() {
        return modelCursor;
    }

    @Override
    public /*@Nullable*/ ModelCursor getDirectionalCursor(
            boolean forward, /*@Nullable*/ String startingChild) {
        return null;
    }

    @Override
    public void registerObserver(FeatureChangeObserver observer) {
        this.observers.add(observer);
    }

    @Override
    public void unregisterObserver(FeatureChangeObserver observer) {
        this.observers.remove(observer);
    }

    public static class Builder {
        private ModelCursor modelCursor = new FakeModelCursor(new ArrayList<>());
        private StreamFeature streamFeature = StreamFeature.getDefaultInstance();

        private Builder() {}

        public Builder setModelCursor(ModelCursor modelCursor) {
            this.modelCursor = modelCursor;
            return this;
        }

        public Builder setStreamFeature(StreamFeature streamFeature) {
            this.streamFeature = streamFeature;
            return this;
        }

        public FakeModelFeature build() {
            return new FakeModelFeature(modelCursor, streamFeature);
        }
    }
}
