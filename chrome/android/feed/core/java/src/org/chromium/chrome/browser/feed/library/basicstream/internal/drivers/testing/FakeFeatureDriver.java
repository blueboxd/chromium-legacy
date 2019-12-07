// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.testing;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.FeatureDriver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.drivers.LeafFeatureDriver;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelFeature;

/** Fake for {@link FeatureDriver}. */
public class FakeFeatureDriver implements FeatureDriver {
    @Nullable
    private final LeafFeatureDriver mLeafFeatureDriver;
    private final ModelFeature mModelFeature;

    private FakeFeatureDriver(
            @Nullable LeafFeatureDriver leafFeatureDriver, ModelFeature modelFeature) {
        this.mLeafFeatureDriver = leafFeatureDriver;
        this.mModelFeature = modelFeature;
    }

    @Override
    public void onDestroy() {}

    @Override
    @Nullable
    public LeafFeatureDriver getLeafFeatureDriver() {
        return mLeafFeatureDriver;
    }

    public ModelFeature getModelFeature() {
        return mModelFeature;
    }

    public static class Builder {
        @Nullable
        private LeafFeatureDriver mLeafFeatureDriver = new FakeLeafFeatureDriver.Builder().build();

        private ModelFeature mModelFeature = FakeModelFeature.newBuilder().build();

        public Builder setLeafFeatureDriver(@Nullable LeafFeatureDriver contentModel) {
            this.mLeafFeatureDriver = contentModel;
            return this;
        }

        public Builder setModelFeature(ModelFeature modelFeature) {
            this.mModelFeature = modelFeature;
            return this;
        }

        public FakeFeatureDriver build() {
            return new FakeFeatureDriver(mLeafFeatureDriver, mModelFeature);
        }
    }
}
