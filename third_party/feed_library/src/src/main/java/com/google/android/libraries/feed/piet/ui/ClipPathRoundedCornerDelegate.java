// Copyright 2019 The Feed Authors.
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

package com.google.android.libraries.feed.piet.ui;

import android.graphics.Canvas;
import android.graphics.Path;
import android.graphics.RectF;
import android.os.Build;
import android.view.ViewGroup;

/**
 * Rounding delegate for the clip path strategy.
 *
 * <p>Adds logic to create a path along the edge of the view and clip to it. {@link
 * RoundedCornerWrapperView} decides which rounding strategy to use and sets the appropriate
 * delegate.
 */
class ClipPathRoundedCornerDelegate extends RoundedCornerDelegate {

  private final Path clipPath = new Path();

  /**
   * Turns off clipping to the outline.
   *
   * <p>This rounding strategy should not be doing outline clipping. This defensively makes sure
   * it's turned off, although that should be the default.
   */
  @Override
  public void initializeForView(ViewGroup view) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
      view.setClipToOutline(false);
      view.setClipChildren(false);
    }
  }

  /** Updates the path every time the size changes. */
  @Override
  public void onSizeChanged(int radius, int width, int height, int bitmask, boolean isRtL) {
    float[] radii = RoundedCornerViewHelper.createRoundedCornerBitMask(radius, bitmask, isRtL);
    clipPath.addRoundRect(new RectF(0, 0, width, height), radii, Path.Direction.CW);
  }

  /** Clips the path that was created in onSizeChanged(). */
  @Override
  public void dispatchDraw(Canvas canvas) {
    if (clipPath != null) {
      canvas.clipPath(clipPath);
    }
  }
}
