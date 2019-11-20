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

package com.google.android.libraries.feed.feedprotocoladapter.internal.transformers;

import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.FeedResponseProto.FeedResponseMetadata;

/**
 * A DataOperationTransformer transform a {@link DataOperation} into a {@link
 * StreamDataOperation.Builder}. DataOperationTranformer(s) can be chained to perform multiple
 * transformations.
 */
public interface DataOperationTransformer {

  /**
   * Transforms a {@link DataOperation} into a {@link StreamDataOperation.Builder}. {@link
   * StreamDataOperation.Builder} is returned to allow for multiple transformations.
   */
  StreamDataOperation.Builder transform(
      DataOperation dataOperation,
      StreamDataOperation.Builder streamDataOperation,
      FeedResponseMetadata feedResponseMetadata);
}
