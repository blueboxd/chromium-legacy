// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedactionparser.internal;

import com.google.android.libraries.feed.api.host.stream.TooltipInfo;
import com.google.search.now.ui.action.FeedActionProto.TooltipData;

/**
 * Implementation for {@link TooltipInfo} that converts a {@link TooltipData} to {@link
 * TooltipInfo}.
 */
public class TooltipInfoImpl implements TooltipInfo {
    private final String label;
    private final String accessibilityLabel;
    @FeatureName
    private final String featureName;
    private final int topInset;
    private final int bottomInset;

    public TooltipInfoImpl(TooltipData tooltipData) {
        this.label = tooltipData.getLabel();
        this.accessibilityLabel = tooltipData.getAccessibilityLabel();
        this.featureName = convert(tooltipData.getFeatureName());
        this.topInset = tooltipData.getInsets().getTop();
        this.bottomInset = tooltipData.getInsets().getBottom();
    }

    /** Converts the type in {@link TooltipData#FeatureName} to {@link TooltipInfo#FeatureName}. */
    @FeatureName
    private static String convert(TooltipData.FeatureName featureName) {
        return (featureName == TooltipData.FeatureName.CARD_MENU) ? FeatureName.CARD_MENU_TOOLTIP
                                                                  : FeatureName.UNKNOWN;
    }

    @Override
    public String getLabel() {
        return label;
    }

    @Override
    public String getAccessibilityLabel() {
        return accessibilityLabel;
    }

    @Override
    @FeatureName
    public String getFeatureName() {
        return featureName;
    }

    @Override
    public int getTopInset() {
        return topInset;
    }

    @Override
    public int getBottomInset() {
        return bottomInset;
    }
}
