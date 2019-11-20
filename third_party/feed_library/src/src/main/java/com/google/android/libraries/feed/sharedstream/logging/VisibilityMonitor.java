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

package com.google.android.libraries.feed.sharedstream.logging;

import android.graphics.Rect;
import android.support.v4.view.ViewCompat;
import android.view.View;
import android.view.ViewParent;
import android.view.ViewTreeObserver;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;

/** Utility class to monitor the visibility of a view. */
public class VisibilityMonitor
    implements ViewTreeObserver.OnPreDrawListener, View.OnAttachStateChangeListener {

  public static final double DEFAULT_VIEW_LOG_THRESHOLD = .66;
  private static final String TAG = "VisibilityMonitor";

  private final View view;
  private final double viewLogThreshold;
  private boolean visible;
  /*@Nullable*/ private VisibilityListener visibilityListener;

  public VisibilityMonitor(View view, Configuration configuration) {
    this.view = view;
    this.viewLogThreshold =
        configuration.getValueOrDefault(ConfigKey.VIEW_LOG_THRESHOLD, DEFAULT_VIEW_LOG_THRESHOLD);
  }

  public VisibilityMonitor(View view, double viewLogThreshold) {
    this.view = view;
    this.viewLogThreshold = viewLogThreshold;
  }

  @Override
  public boolean onPreDraw() {
    ViewParent parent = view.getParent();
    if (parent != null) {
      Rect rect = new Rect(0, 0, view.getWidth(), view.getHeight());

      @SuppressWarnings("argument.type.incompatible")
      boolean childVisibleRectNotEmpty = parent.getChildVisibleRect(view, rect, null);
      if (childVisibleRectNotEmpty && rect.height() >= viewLogThreshold * view.getHeight()) {
        if (!visible) {
          notifyListenerOnVisible();
          visible = true;
        }
      } else {
        visible = false;
      }
    }
    return true;
  }

  @Override
  public void onViewAttachedToWindow(View v) {
    view.getViewTreeObserver().addOnPreDrawListener(this);
  }

  @Override
  public void onViewDetachedFromWindow(View v) {
    visible = false;
    view.getViewTreeObserver().removeOnPreDrawListener(this);
  }

  public void setListener(/*@Nullable*/ VisibilityListener visibilityListener) {
    if (visibilityListener != null) {
      detach();
    }

    this.visibilityListener = visibilityListener;

    if (visibilityListener != null) {
      attach();
    }
  }

  private void attach() {
    view.addOnAttachStateChangeListener(this);
    if (ViewCompat.isAttachedToWindow(view)) {
      view.getViewTreeObserver().addOnPreDrawListener(this);
    }
  }

  private void detach() {
    view.removeOnAttachStateChangeListener(this);
    if (ViewCompat.isAttachedToWindow(view)) {
      view.getViewTreeObserver().removeOnPreDrawListener(this);
    }
  }

  private void notifyListenerOnVisible() {
    if (visibilityListener != null) {
      visibilityListener.onViewVisible();
    }
  }
}
