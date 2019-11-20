// Copyright 2019 The Feed Authors.
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

package com.google.android.libraries.feed.feedstore.internal;

import com.google.android.libraries.feed.common.intern.Interner;
import com.google.android.libraries.feed.common.intern.ProtoStringInternerBase;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;

/**
 * Interner that interns content ID related strings from StreamStructure protos. The reason is that
 * there is a great potential to reuse there, many content IDs are identical.
 */
public class StreamStructureInterner extends ProtoStringInternerBase<StreamStructure> {

  StreamStructureInterner(Interner<String> interner) {
    super(interner);
  }

  @Override
  public StreamStructure intern(StreamStructure input) {
    StreamStructure.Builder builder = null;
    builder =
        internSingleStringField(
            input, builder, StreamStructure::getContentId, StreamStructure.Builder::setContentId);
    builder =
        internSingleStringField(
            input,
            builder,
            StreamStructure::getParentContentId,
            StreamStructure.Builder::setParentContentId);

    // If builder is not null we memoized something and  we need to replace the proto with the
    // modified proto.
    return (builder != null) ? builder.build() : input;
  }
}
