// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.piet;

import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.RequiredContentAdapter;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.PietContent;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation.Operation;

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
