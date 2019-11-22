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
    private final ArrayList<String> unsupportedFeatures = new ArrayList<>();
    private final ThreadUtils threadUtils;
    /*@Nullable*/ private String lastFeatureName;

    public FakeTooltipSupportedApi(ThreadUtils threadUtils) {
        this.threadUtils = threadUtils;
    }

    @Override
    public void wouldTriggerHelpUi(@FeatureName String featureName, Consumer<Boolean> consumer) {
        threadUtils.checkMainThread();
        lastFeatureName = featureName;
        consumer.accept(!unsupportedFeatures.contains(featureName));
    }

    /** Adds an unsupported feature. */
    public FakeTooltipSupportedApi addUnsupportedFeature(String featureName) {
        unsupportedFeatures.add(featureName);
        return this;
    }

    /**
     * Gets the last feature name that was passed in to {@link wouldTriggerHelpUi(String,
     * Consumer)}.
     */
    /*@Nullable*/
    public String getLatestFeatureName() {
        return lastFeatureName;
    }
}
