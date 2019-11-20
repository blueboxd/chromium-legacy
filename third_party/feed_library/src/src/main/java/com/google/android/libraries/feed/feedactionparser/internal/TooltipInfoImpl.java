// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
  @FeatureName private final String featureName;
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
    return (featureName == TooltipData.FeatureName.CARD_MENU)
        ? FeatureName.CARD_MENU_TOOLTIP
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
