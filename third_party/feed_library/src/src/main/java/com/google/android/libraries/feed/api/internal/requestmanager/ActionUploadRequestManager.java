// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.requestmanager;

import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;

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
