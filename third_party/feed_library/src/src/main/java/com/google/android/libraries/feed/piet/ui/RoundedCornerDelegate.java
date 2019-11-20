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
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

/**
 * Abstract delegate that allows its children to add logic to round the corners of a {@link View}.
 *
 * <p>This class is abstract as it is not meant to be directly instantiated. Multiple delegates with
 * different rounding strategies subclass this delegate. Each delegate can add logic to different
 * {@link View} lifecycle methods, as well as initialize and destroy logic. Since {@link
 * RoundedCornerWrapperView} actually inherits from {@link View}, it takes care of calling the super
 * methods of these lifecycle events, in addition to calling these delegate methods.
 *
 * <p>Since not all rounding strategies need to override all of these methods, and {@link
 * RoundedCornerWrapperView} calls the default 'super' implementation, they are left as empty
 * methods by default. Having empty methods here simplifies the logic in {@link
 * RoundedCornerWrapperView}, because that view doesn't need to know which strategies require extra
 * logic in specific methods.
 */
abstract class RoundedCornerDelegate {

  /**
   * Initializes anything that needs to be changed on the view that is being rounded.
   *
   * <p>This should be the opposite of what is torn down in destroy().
   */
  void initializeForView(ViewGroup view) {}

  /** Allows a rounded corner delegate to add logic when child views are drawn. */
  void dispatchDraw(Canvas canvas) {}

  /** Allows a rounded corner delegate to add logic during layout. */
  void onLayout(int radius, boolean isRtL, int width, int height) {}

  /**
   * Allows a rounded corner delegate to add logic during {@link
   * ViewParent#onDescendantInvalidated}.
   */
  void onDescendantInvalidated(View roundedCornerView, View invalidatedDescendant) {}

  /**
   * Allows a rounded corner delegate to add logic during {@link
   * ViewParent#invalidateChildInParent}.
   */
  void invalidateChildInParent(View view, final Rect dirty) {}

  /** Allows a rounded corner delegate to add logic when the size changes. */
  void onSizeChanged(int radius, int width, int height, int bitmask, boolean isRtL) {}

  /**
   * Renders the view to this canvas.
   *
   * <p>The default call to the super method is here, rather than in {@link
   * RoundedCornerWrapperView}, because an implementation may override this and set a bitmap
   * directly to the canvas.
   *
   * <p>If this is overridden, view.drawSuper() must be called, even if it is with a different
   * {@link Canvas}.
   */
  void draw(RoundedCornerWrapperView view, Canvas canvas) {
    view.drawSuper(canvas);
  }

  /**
   * Removes any modifications the delegate has made to the view that is being rounded.
   *
   * <p>This should be called on the current {@link RoundedCornerDelegate} when switching from one
   * delegate to another.
   */
  void destroy(ViewGroup view) {}
}
