// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedactionparser.internal;

import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipInfo;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.TooltipData;

/**
 * Implementation for {@link TooltipInfo} that converts a {@link TooltipData} to {@link
 * TooltipInfo}.
 */
public class TooltipInfoImpl implements TooltipInfo {
    private final String mLabel;
    private final String mAccessibilityLabel;
    @FeatureName
    private final String mFeatureName;
    private final int mTopInset;
    private final int mBottomInset;

    public TooltipInfoImpl(TooltipData tooltipData) {
        this.mLabel = tooltipData.getLabel();
        this.mAccessibilityLabel = tooltipData.getAccessibilityLabel();
        this.mFeatureName = convert(tooltipData.getFeatureName());
        this.mTopInset = tooltipData.getInsets().getTop();
        this.mBottomInset = tooltipData.getInsets().getBottom();
    }

    /** Converts the type in {@link TooltipData#FeatureName} to {@link TooltipInfo#FeatureName}. */
    @FeatureName
    private static String convert(TooltipData.FeatureName featureName) {
        return (featureName == TooltipData.FeatureName.CARD_MENU) ? FeatureName.CARD_MENU_TOOLTIP
                                                                  : FeatureName.UNKNOWN;
    }

    @Override
    public String getLabel() {
        return mLabel;
    }

    @Override
    public String getAccessibilityLabel() {
        return mAccessibilityLabel;
    }

    @Override
    @FeatureName
    public String getFeatureName() {
        return mFeatureName;
    }

    @Override
    public int getTopInset() {
        return mTopInset;
    }

    @Override
    public int getBottomInset() {
        return mBottomInset;
    }
}
