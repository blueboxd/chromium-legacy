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

package com.google.android.libraries.feed.sharedstream.piet;

import android.graphics.drawable.Drawable;
import com.google.android.libraries.feed.api.host.imageloader.ImageLoaderApi;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.piet.host.ImageLoader;
import com.google.search.now.ui.piet.ImagesProto.Image;
import com.google.search.now.ui.piet.ImagesProto.ImageSource;
import java.util.ArrayList;
import java.util.List;
import javax.inject.Inject;

/** A Piet {@link ImageLoader} which uses {@link ImageLoaderApi} to load images. */
public class PietImageLoader implements ImageLoader {

  private final ImageLoaderApi imageLoaderApi;

  @Inject
  public PietImageLoader(ImageLoaderApi imageLoaderApi) {
    this.imageLoaderApi = imageLoaderApi;
  }

  @Override
  public void getImage(
      Image image, int widthPx, int heightPx, Consumer</*@Nullable*/ Drawable> consumer) {
    List<String> urls = new ArrayList<>(image.getSourcesList().size());
    for (ImageSource source : image.getSourcesList()) {
      urls.add(source.getUrl());
    }

    imageLoaderApi.loadDrawable(urls, widthPx, heightPx, consumer);
  }
}
