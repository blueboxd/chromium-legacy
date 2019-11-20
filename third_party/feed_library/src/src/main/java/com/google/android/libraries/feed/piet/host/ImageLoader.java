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

import android.graphics.drawable.Drawable;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.search.now.ui.piet.ImagesProto.Image;

/** Class that allows Piet to ask the host to load an image. */
public interface ImageLoader {

  /**
   * Given an {@link Image}, asynchronously load the {@link Drawable} and return via a {@link
   * Consumer}.
   *
   * <p>The width and the height of the image can be provided preemptively, however it is not
   * guaranteed that both dimensions will be known. In the case that only one dimension is known,
   * the host should be careful to preserve the aspect ratio.
   *
   * @param image The image to load.
   * @param widthPx The width of the {@link Image} in pixels. Will be {@link #DIMENSION_UNKNOWN} if
   *     unknown.
   * @param heightPx The height of the {@link Image} in pixels. Will be {@link #DIMENSION_UNKNOWN}
   *     if unknown.
   * @param consumer Callback to return the {@link Drawable} from an {@link Image} if the load
   *     succeeds. {@literal null} should be passed to this if no source succeeds in loading the
   *     image
   */
  void getImage(Image image, int widthPx, int heightPx, Consumer</*@Nullable*/ Drawable> consumer);
}
