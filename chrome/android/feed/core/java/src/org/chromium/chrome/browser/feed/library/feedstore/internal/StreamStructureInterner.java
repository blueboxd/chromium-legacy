// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import org.chromium.chrome.browser.feed.library.common.intern.Interner;
import org.chromium.chrome.browser.feed.library.common.intern.ProtoStringInternerBase;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;

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
        builder = internSingleStringField(input, builder, StreamStructure::getContentId,
                StreamStructure.Builder::setContentId);
        builder = internSingleStringField(input, builder, StreamStructure::getParentContentId,
                StreamStructure.Builder::setParentContentId);

        // If builder is not null we memoized something and  we need to replace the proto with the
        // modified proto.
        return (builder != null) ? builder.build() : input;
    }
}
