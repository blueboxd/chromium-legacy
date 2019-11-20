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


/**
 * Class to represent the width and height of a view in pixels. This is a minimal version of {@link
 * android.util.Size} which is only available in API 21.
 */
public class Size {

  private final int width;
  private final int height;

  public Size(int width, int height) {
    this.width = width;
    this.height = height;
  }

  /** Gets the width of the size in pixels. */
  public int getWidth() {
    return width;
  }

  /** Gets the height of the size in pixels. */
  public int getHeight() {
    return height;
  }

  @Override
  public boolean equals(/*@Nullable*/ Object o) {
    if (this == o) {
      return true;
    }
    if (!(o instanceof Size)) {
      return false;
    }

    Size size = (Size) o;

    return width == size.width && height == size.height;
  }

  @Override
  public int hashCode() {
    return 31 * width + height;
  }
}
