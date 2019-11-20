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

package com.google.android.libraries.feed.sharedstream.publicapi.menumeasurer;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListAdapter;
import java.util.ArrayList;
import java.util.List;

/** Makes measurements for a Menu based on an {@link ArrayAdapter}. */
public class MenuMeasurer {

  public static final int NO_MAX_HEIGHT = Integer.MAX_VALUE;
  public static final int NO_MAX_WIDTH = Integer.MAX_VALUE;
  private static final String TAG = "MenuMeasurer";

  private final Context context;

  public MenuMeasurer(Context context) {
    this.context = context;
  }

  // TODO: Test measureAdapterContent fully instead of just calculateSize.
  public Size measureAdapterContent(
      ViewGroup parent, ListAdapter adapter, int windowPadding, int maxWidth, int maxHeight) {
    return calculateSize(getMeasurements(parent, adapter), windowPadding, maxWidth, maxHeight);
  }

  private List<Size> getMeasurements(ViewGroup parent, ListAdapter adapter) {
    ArrayList<Size> measurements = new ArrayList<>(adapter.getCount());
    int widthMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
    int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
    View view = null;
    for (int i = 0; i < adapter.getCount(); i++) {
      view = getViewFromAdapter(adapter, i, view, parent);
      view.measure(widthMeasureSpec, heightMeasureSpec);
      measurements.add(new Size(view.getMeasuredWidth(), view.getMeasuredHeight()));
    }
    return measurements;
  }

  @VisibleForTesting
  Size calculateSize(List<Size> measuredSizes, int windowPadding, int maxWidth, int maxHeight) {
    int largestWidth = 0;
    int totalHeight = 0;
    for (Size size : measuredSizes) {
      int itemWidth = size.getWidth();
      totalHeight += size.getHeight();

      if (itemWidth > largestWidth) {
        largestWidth = itemWidth;
      }
    }

    int widthUnit = context.getResources().getDimensionPixelSize(R.dimen.menu_width_multiple);

    int width =
        Math.min(
            roundLargestPopupContentWidth(largestWidth, widthUnit),
            maxWidth - windowPadding - windowPadding);
    return new Size(width, Math.min(totalHeight + windowPadding + windowPadding, maxHeight));
  }

  /**
   * Given the {@code largestWidth} of members of popup content and the {@code widthUnit}, returns
   * the smallest multiple of {@code widthUnit} that is at least as large as {@code largestWidth}.
   */
  private int roundLargestPopupContentWidth(int largestWidth, int widthUnit) {
    return Math.round((((float) largestWidth / (float) widthUnit) + 0.5f)) * widthUnit;
  }

  /** Minimal method to suppress an incorrect nullness error. */
  @SuppressWarnings("nullness:argument.type.incompatible")
  private View getViewFromAdapter(
      ListAdapter arrayAdapter, int index, /*@Nullable*/ View convertView, ViewGroup parentView) {
    return arrayAdapter.getView(index, convertView, parentView);
  }
}
