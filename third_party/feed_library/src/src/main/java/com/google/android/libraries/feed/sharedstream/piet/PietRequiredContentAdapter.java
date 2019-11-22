// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

        return dataOperation.getFeature()
                .getExtension(Content.contentExtension)
                .getExtension(PietContent.pietContentExtension)
                .getPietSharedStatesList();
    }
}
