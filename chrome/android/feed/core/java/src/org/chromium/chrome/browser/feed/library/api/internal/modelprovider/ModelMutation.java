// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;

/**
 * Mutation which will batch a set of atomic changes to the Model maintained by the ModelProvider.
 * Order is important, things will be stored in the order they are added to a parent feature.
 */
public interface ModelMutation {
    /** Add the structural information to the model as a child */
    ModelMutation addChild(StreamStructure streamStructure);

    /** Remove the model child defined by the structural information */
    ModelMutation removeChild(StreamStructure streamStructure);

    /** Content for a child was updated. */
    ModelMutation updateChild(StreamStructure streamStructure);

    /**
     * Set the MutationContext used to create the ModelMutation. This may contain the {@link
     * StreamToken} which represents the source of the response.
     */
    ModelMutation setMutationContext(MutationContext mutationContext);

    /** Set the session id of the session backing this ModelProvider. */
    ModelMutation setSessionId(String sessionId);

    /** Indicates that the SessionManager has cached the payload bindings. */
    ModelMutation hasCachedBindings(boolean cachedBindings);

    /** Commits the pending changes to the store. */
    void commit();
}
