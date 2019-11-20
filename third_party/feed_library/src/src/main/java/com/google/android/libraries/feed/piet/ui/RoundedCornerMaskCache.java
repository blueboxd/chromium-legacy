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

package com.google.android.libraries.feed.piet.ui;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffXfermode;
import android.support.annotation.IntDef;
import android.support.annotation.VisibleForTesting;
import android.util.LruCache;

/** Caches rounded corner masks to save memory and time spent creating them */
public class RoundedCornerMaskCache {
  // TODO: Make cache size configurable.
  private static final int CACHE_SIZE = 8;

  @IntDef({Corner.TOP_LEFT, Corner.TOP_RIGHT, Corner.BOTTOM_LEFT, Corner.BOTTOM_RIGHT})
  @interface Corner {
    int TOP_LEFT = 0;
    int TOP_RIGHT = 1;
    int BOTTOM_LEFT = 2;
    int BOTTOM_RIGHT = 3;
  }

  private final Canvas canvas;
  private final Paint paint;
  private final Paint maskPaint;
  private final LruCache<Integer, RoundedCornerBitmaps> cache;

  public RoundedCornerMaskCache() {
    paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    maskPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    maskPaint.setXfermode(new PorterDuffXfermode(Mode.CLEAR));

    canvas = new Canvas();
    cache =
        new LruCache<Integer, RoundedCornerBitmaps>(CACHE_SIZE) {
          @Override
          protected RoundedCornerBitmaps create(Integer key) {
            return new RoundedCornerBitmaps(key, canvas, maskPaint);
          }
        };
  }

  Paint getPaint() {
    return paint;
  }

  Paint getMaskPaint() {
    return maskPaint;
  }

  RoundedCornerBitmaps getMasks(int radius) {
    RoundedCornerBitmaps masks = cache.get(radius);
    if (masks == null) {
      masks = new RoundedCornerBitmaps(radius, canvas, maskPaint);
      cache.put(radius, masks);
    }
    return masks;
  }

  public void purge() {
    cache.evictAll();
  }

  static class RoundedCornerBitmaps {
    @VisibleForTesting final Bitmap[] masks;
    private final Canvas canvas;
    private final int radius;
    private final Paint maskPaint;

    RoundedCornerBitmaps(int radius, Canvas canvas, Paint maskPaint) {
      masks = new Bitmap[4];
      this.canvas = canvas;
      this.radius = radius;
      this.maskPaint = maskPaint;
    }

    Bitmap get(@Corner int corner) {
      switch (corner) {
        case Corner.TOP_LEFT:
          if (masks[Corner.TOP_LEFT] == null) {
            masks[Corner.TOP_LEFT] = Bitmap.createBitmap(radius, radius, Config.ALPHA_8);
            canvas.setBitmap(masks[Corner.TOP_LEFT]);
            canvas.drawColor(Color.BLACK);
            canvas.drawCircle(radius, radius, radius, maskPaint);
          }
          return masks[Corner.TOP_LEFT];

        case Corner.TOP_RIGHT:
          if (masks[Corner.TOP_RIGHT] == null) {
            masks[Corner.TOP_RIGHT] = Bitmap.createBitmap(radius, radius, Config.ALPHA_8);
            canvas.setBitmap(masks[Corner.TOP_RIGHT]);
            canvas.drawColor(Color.BLACK);
            canvas.drawCircle(0, radius, radius, maskPaint);
          }
          return masks[Corner.TOP_RIGHT];

        case Corner.BOTTOM_LEFT:
          if (masks[Corner.BOTTOM_LEFT] == null) {
            masks[Corner.BOTTOM_LEFT] = Bitmap.createBitmap(radius, radius, Config.ALPHA_8);
            canvas.setBitmap(masks[Corner.BOTTOM_LEFT]);
            canvas.drawColor(Color.BLACK);
            canvas.drawCircle(radius, 0, radius, maskPaint);
          }
          return masks[Corner.BOTTOM_LEFT];

        case Corner.BOTTOM_RIGHT:
          if (masks[Corner.BOTTOM_RIGHT] == null) {
            masks[Corner.BOTTOM_RIGHT] = Bitmap.createBitmap(radius, radius, Config.ALPHA_8);
            canvas.setBitmap(masks[Corner.BOTTOM_RIGHT]);
            canvas.drawColor(Color.BLACK);
            canvas.drawCircle(0, 0, radius, maskPaint);
          }
          return masks[Corner.BOTTOM_RIGHT];

        default:
          throw new IllegalArgumentException(String.format("Unrecognized corner: %s", corner));
      }
    }
  }
}
