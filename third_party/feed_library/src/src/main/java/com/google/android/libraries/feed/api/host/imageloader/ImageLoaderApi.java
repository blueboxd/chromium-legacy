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

package com.google.android.libraries.feed.api.host.imageloader;

import android.graphics.drawable.Drawable;
import com.google.android.libraries.feed.common.functional.Consumer;
import java.util.List;

/** Feed Host API to load images. */
public interface ImageLoaderApi {
  /** Constant used to notify host that an image's height or width is not known. */
  int DIMENSION_UNKNOWN = -1;

  /**
   * Asks host to load an image from the web, a bundled asset, or some other type of drawable like a
   * monogram if supported.
   *
   * <p>The width and the height of the image can be provided preemptively, however it is not
   * guaranteed that both dimensions will be known. In the case that only one dimension is known,
   * the host should be careful to preserve the aspect ratio.
   *
   * <p>Feed will not cache any images, so if caching is desired, it should be done on the host
   * side.
   *
   * @param urls A list of urls, tried in order until a load succeeds. Urls may be bundled. The list
   *     of bundled assets will be defined via the {@link BundledAssets} StringDef.
   * @param widthPx The width of the image in pixels. Will be {@link #DIMENSION_UNKNOWN} if unknown.
   * @param heightPx The height of the image in pixels. Will be {@link #DIMENSION_UNKNOWN} if
   *     unknown.
   * @param consumer Callback to return the Drawable to if one of the urls provided is successful.
   *     {@literal null} if no Drawable is found (or could not be loaded) after trying all possible
   *     urls.
   */
  void loadDrawable(
      List<String> urls, int widthPx, int heightPx, Consumer</*@Nullable*/ Drawable> consumer);
}
