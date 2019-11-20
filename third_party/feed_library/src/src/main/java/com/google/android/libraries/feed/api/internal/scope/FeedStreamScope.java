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

package com.google.android.libraries.feed.api.internal.scope;

import com.google.android.libraries.feed.api.client.scope.StreamScope;
import com.google.android.libraries.feed.api.client.stream.Stream;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;

/** Per-stream instance of the feed library. */
public final class FeedStreamScope implements StreamScope {

  private final Stream stream;
  private final ModelProviderFactory modelProviderFactory;

  public FeedStreamScope(Stream stream, ModelProviderFactory modelProviderFactory) {
    this.stream = stream;
    this.modelProviderFactory = modelProviderFactory;
  }

  public Stream getStream() {
    return stream;
  }

  public ModelProviderFactory getModelProviderFactory() {
    return modelProviderFactory;
  }
}
