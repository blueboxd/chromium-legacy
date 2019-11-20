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

package com.google.android.libraries.feed.sharedstream.piet;

import com.google.android.libraries.feed.api.internal.protocoladapter.RequiredContentAdapter;
import com.google.search.now.ui.stream.StreamStructureProto.Content;
import com.google.search.now.ui.stream.StreamStructureProto.PietContent;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation.Operation;
import java.util.Collections;
import java.util.List;

/** Implementation of {@link RequiredContentAdapter} that identifies dependent PietSharedStates. */
public final class PietRequiredContentAdapter implements RequiredContentAdapter {
  @Override
  public List<ContentId> determineRequiredContentIds(DataOperation dataOperation) {
    if (dataOperation.getOperation() != Operation.UPDATE_OR_APPEND) {
      return Collections.emptyList();
    }

    return dataOperation
        .getFeature()
        .getExtension(Content.contentExtension)
        .getExtension(PietContent.pietContentExtension)
        .getPietSharedStatesList();
  }
}
