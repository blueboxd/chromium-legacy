// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.host.stream;

import com.google.android.libraries.feed.api.host.stream.TooltipInfo.FeatureName;
import com.google.android.libraries.feed.api.host.stream.TooltipSupportedApi;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.common.functional.Consumer;

import java.util.ArrayList;

/** Fake implementation of {@link TooltipSupportedApi}. */
public final class FakeTooltipSupportedApi implements TooltipSupportedApi {
    private final ArrayList<String> mUnsupportedFeatures = new ArrayList<>();
    private final ThreadUtils mThreadUtils;
    /*@Nullable*/ private String mLastFeatureName;

    public FakeTooltipSupportedApi(ThreadUtils threadUtils) {
        this.mThreadUtils = threadUtils;
    }

    @Override
    public void wouldTriggerHelpUi(@FeatureName String featureName, Consumer<Boolean> consumer) {
        mThreadUtils.checkMainThread();
        mLastFeatureName = featureName;
        consumer.accept(!mUnsupportedFeatures.contains(featureName));
    }

    /** Adds an unsupported feature. */
    public FakeTooltipSupportedApi addUnsupportedFeature(String featureName) {
        mUnsupportedFeatures.add(featureName);
        return this;
    }

    /**
     * Gets the last feature name that was passed in to {@link wouldTriggerHelpUi(String,
     * Consumer)}.
     */
    /*@Nullable*/
    public String getLatestFeatureName() {
        return mLastFeatureName;
    }
}
