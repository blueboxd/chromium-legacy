// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.requestmanager;

import com.google.android.libraries.feed.api.internal.requestmanager.ActionUploadRequestManager;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;

import java.util.HashSet;
import java.util.Set;

/** Fake implements of {@link ActionUploadRequestManager}. */
public final class FakeActionUploadRequestManager implements ActionUploadRequestManager {
    private final FakeThreadUtils fakeThreadUtils;
    private Result<ConsistencyToken> result = Result.success(ConsistencyToken.getDefaultInstance());
    /*@Nullable*/ private Set<StreamUploadableAction> actions;

    public FakeActionUploadRequestManager(FakeThreadUtils fakeThreadUtils) {
        this.fakeThreadUtils = fakeThreadUtils;
    }

    @Override
    public void triggerUploadActions(Set<StreamUploadableAction> actions, ConsistencyToken token,
            Consumer<Result<ConsistencyToken>> consumer) {
        this.actions = actions;
        boolean policy = fakeThreadUtils.enforceMainThread(false);
        try {
            consumer.accept(result);
        } finally {
            fakeThreadUtils.enforceMainThread(policy);
        }
    }

    /**
     * Sets the result to return from triggerUploadActions. If unset will use {@code
     * Result.success(ConsistencyToken.getDefaultInstance())}.
     */
    public FakeActionUploadRequestManager setResult(Result<ConsistencyToken> result) {
        this.result = result;
        return this;
    }

    /** Returns the last set of actions sent to triggerUploadActions. */
    public Set<StreamUploadableAction> getLatestActions() {
        if (actions == null) {
            return new HashSet<>();
        }

        return actions;
    }
}
