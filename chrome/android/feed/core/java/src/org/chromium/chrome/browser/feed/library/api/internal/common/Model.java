// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.common;

import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;

import java.util.ArrayList;
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
                Collections.unmodifiableList(new ArrayList(streamDataOperations));
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
