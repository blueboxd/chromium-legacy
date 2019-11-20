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

package com.google.android.libraries.feed.api.internal.common;

import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import java.util.Collections;
import java.util.List;

/** Contains a list of {@link StreamDataOperations}s and a schema version. */
public final class Model {
  /** The current schema version. */
  public static final int CURRENT_SCHEMA_VERSION = 2;

  public final List<StreamDataOperation> streamDataOperations;
  public final int schemaVersion;

  private Model(List<StreamDataOperation> streamDataOperations, int schemaVersion) {
    this.streamDataOperations =
        Collections.unmodifiableList(
            Collections.list(Collections.enumeration(streamDataOperations)));
    this.schemaVersion = schemaVersion;
  }

  public static Model of(List<StreamDataOperation> streamDataOperations) {
    return of(streamDataOperations, CURRENT_SCHEMA_VERSION);
  }

  public static Model of(List<StreamDataOperation> streamDataOperations, int schemaVersion) {
    return new Model(streamDataOperations, schemaVersion);
  }

  public static Model empty() {
    return new Model(Collections.emptyList(), CURRENT_SCHEMA_VERSION);
  }
}
