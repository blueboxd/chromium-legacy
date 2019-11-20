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

package com.google.android.libraries.feed.api.internal.actionparser;

import com.google.android.libraries.feed.api.client.knowncontent.ContentMetadata;
import com.google.android.libraries.feed.common.functional.Supplier;

/** Factory for {@link ActionParser}. */
public interface ActionParserFactory {

  /**
   * Builds the ActionParser.
   *
   * @param contentMetadata A {@link Supplier} for {@link ContentMetadata} required for the {@link
   *     com.google.android.libraries.feed.api.host.action.ActionApi#downloadUrl(ContentMetadata)}
   *     action. If the {@link Supplier} returns {@code null}, the download action will be
   *     suppressed. A {@link Supplier} is used instead of just a {@link ContentMetadata} to save on
   *     memory and startup time. The {@link Supplier} will not be accessed until an action is taken
   *     that requires it.
   */
  ActionParser build(Supplier</*@Nullable*/ ContentMetadata> contentMetadata);
}
