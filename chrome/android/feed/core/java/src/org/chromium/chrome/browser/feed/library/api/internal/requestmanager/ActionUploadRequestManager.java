// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.requestmanager;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;

import java.util.Set;

/** Creates and issues upload action requests to the server. */
public interface ActionUploadRequestManager {
    /**
     * Issues a request to record a set of actions.
     *
     * <p>The provided {@code consumer} will be executed on a background thread.
     */
    void triggerUploadActions(Set<StreamUploadableAction> actions, ConsistencyToken token,
            Consumer<Result<ConsistencyToken>> consumer);
}
