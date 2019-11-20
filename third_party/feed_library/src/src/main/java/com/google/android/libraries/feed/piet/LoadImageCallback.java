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

package com.google.android.libraries.feed.piet;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;
import com.google.android.libraries.feed.common.functional.Consumer;

/**
 * Handles loading images from the host. In particular, handles the resizing of images as well as
 * the fading animation.
 */
public class LoadImageCallback implements Consumer</*@Nullable*/ Drawable> {

  static final int FADE_IN_ANIMATION_TIME_MS = 300;

  private final ImageView imageView;
  private final ScaleType scaleType;
  private final long initialTime;
  /*@Nullable*/ private final Integer overlayColor;
  private final boolean fadeImage;
  private final AdapterParameters parameters;

  private boolean cancelled;

  /*@Nullable*/ private Drawable finalDrawable;

  LoadImageCallback(
      ImageView imageView,
      ScaleType scaleType,
      /*@Nullable*/ Integer overlayColor,
      boolean fadeImage,
      AdapterParameters parameters,
      FrameContext frameContext) {
    this.imageView = imageView;
    this.scaleType = scaleType;
    this.overlayColor = overlayColor;
    this.fadeImage = fadeImage;
    this.parameters = parameters;
    this.initialTime = parameters.clock.elapsedRealtime();
  }

  @Override
  public void accept(/*@Nullable*/ Drawable drawable) {
    if (cancelled || drawable == null) {
      return;
    }

    imageView.setScaleType(scaleType);

    final Drawable localDrawable = ViewUtils.applyOverlayColor(drawable, overlayColor);
    this.finalDrawable = localDrawable;

    // If we are in the process of binding when we get the image, we should not fade in the
    // image as the image was cached.
    if (!shouldFadeInImage()) {
      imageView.setImageDrawable(localDrawable);
      // Invalidating the view as the view doesn't update if not manually updated here.
      imageView.invalidate();
      return;
    }

    Drawable initialDrawable =
        imageView.getDrawable() != null
            ? imageView.getDrawable()
            : new ColorDrawable(Color.TRANSPARENT);

    TransitionDrawable transitionDrawable =
        new TransitionDrawable(new Drawable[] {initialDrawable, localDrawable});
    imageView.setImageDrawable(transitionDrawable);
    transitionDrawable.setCrossFadeEnabled(true);
    transitionDrawable.startTransition(FADE_IN_ANIMATION_TIME_MS);

    imageView.postDelayed(
        () -> {
          if (cancelled) {
            return;
          }
          // Allows GC of the initial drawable and the transition drawable. Additionally
          // fixes the issue where the transition sometimes doesn't occur, which would
          // result in blank images.
          imageView.setImageDrawable(this.finalDrawable);
        },
        FADE_IN_ANIMATION_TIME_MS);
  }

  private boolean shouldFadeInImage() {
    return fadeImage
        && (parameters.clock.elapsedRealtime() - initialTime)
            > parameters.hostProviders.getAssetProvider().getFadeImageThresholdMs();
  }

  void cancel() {
    this.cancelled = true;
  }
}
