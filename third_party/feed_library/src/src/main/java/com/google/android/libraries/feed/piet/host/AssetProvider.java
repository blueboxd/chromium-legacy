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

package com.google.android.libraries.feed.piet.host;

import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.support.annotation.VisibleForTesting;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.piet.host.TypefaceProvider.GoogleSansTypeface;
import com.google.search.now.ui.piet.ImagesProto.Image;

/** Provide Assets from the host */
public class AssetProvider {
  @VisibleForTesting final ImageLoader imageLoader;
  @VisibleForTesting final StringFormatter stringFormatter;
  private final Supplier<Integer> defaultCornerRadiusSupplier;
  private final Supplier<Boolean> isDarkThemeSupplier;
  private final Supplier<Long> fadeImageThresholdMsSupplier;
  private final Supplier<Boolean> isRtLSupplier;
  @VisibleForTesting final TypefaceProvider typefaceProvider;

  public AssetProvider(
      ImageLoader imageLoader,
      StringFormatter stringFormatter,
      Supplier<Integer> defaultCornerRadiusSupplier,
      Supplier<Long> fadeImageThresholdMsSupplier,
      Supplier<Boolean> isDarkThemeSupplier,
      Supplier<Boolean> isRtLSupplier,
      TypefaceProvider typefaceProvider) {
    this.imageLoader = imageLoader;
    this.stringFormatter = stringFormatter;
    this.defaultCornerRadiusSupplier = defaultCornerRadiusSupplier;
    this.isDarkThemeSupplier = isDarkThemeSupplier;
    this.fadeImageThresholdMsSupplier = fadeImageThresholdMsSupplier;
    this.isRtLSupplier = isRtLSupplier;
    this.typefaceProvider = typefaceProvider;
  }

  /**
   * Given an {@link Image}, asynchronously load the {@link Drawable} and return via a {@link
   * Consumer}.
   *
   * <p>The width and the height of the image can be provided preemptively, however it is not
   * guaranteed that both dimensions will be known. In the case that only one dimension is known,
   * the host should be careful to preserve the aspect ratio.
   *
   * @param image The image to load.
   * @param widthPx The width of the {@link Image} in pixels. Will be {@link #DIMENSION_NOT_SET} if
   *     unknown.
   * @param heightPx The height of the {@link Image} in pixels. Will be {@link #DIMENSION_NOT_SET}
   *     if unknown.
   * @param consumer Callback to return the {@link Drawable} from an {@link Image} if the load
   *     succeeds. {@literal null} should be passed to this if no source succeeds in loading the
   *     image
   */
  public void getImage(
      Image image, int widthPx, int heightPx, Consumer</*@Nullable*/ Drawable> consumer) {
    imageLoader.getImage(image, widthPx, heightPx, consumer);
  }

  /** Return a relative elapsed time string such as "8 minutes ago" or "1 day ago". */
  public String getRelativeElapsedString(long elapsedTimeMillis) {
    return stringFormatter.getRelativeElapsedString(elapsedTimeMillis);
  }

  /** Returns the default corner rounding radius in pixels. */
  public int getDefaultCornerRadius() {
    return defaultCornerRadiusSupplier.get();
  }

  /** Returns whether the theme for the Piet rendering context is a "dark theme". */
  public boolean isDarkTheme() {
    return isDarkThemeSupplier.get();
  }

  /**
   * Fade-in animation will only occur if image loading time takes more than this amount of time.
   */
  public long getFadeImageThresholdMs() {
    return fadeImageThresholdMsSupplier.get();
  }

  /**
   * Allows the host to return a typeface Piet would otherwise not be able to access (ex. from
   * assets). Piet will call this when the typeface is not one Piet recognizes (as a default Android
   * typeface). If host does not specially handle the specified typeface, host can return {@code
   * null}, and Piet will proceed through its fallback typefaces.
   *
   * <p>Piet also expects the host to provide the Google Sans typeface, and will request it using
   * the {@link GoogleSansTypeface} StringDef. Piet will report errors if Google Sans is requested
   * and not found.
   */
  public void getTypeface(
      String typeface, boolean isItalic, Consumer</*@Nullable*/ Typeface> consumer) {
    typefaceProvider.getTypeface(typeface, isItalic, consumer);
  }

  /** Returns whether Piet should render layouts using a right-to-left orientation. */
  public boolean isRtL() {
    return isRtLSupplier.get();
  }

  /** Returns whether Piet should render layouts using a right-to-left orientation. */
  public Supplier<Boolean> isRtLSupplier() {
    return isRtLSupplier;
  }
}
