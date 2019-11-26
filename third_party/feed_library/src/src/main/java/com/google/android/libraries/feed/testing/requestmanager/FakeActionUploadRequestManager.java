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
    private final FakeThreadUtils mFakeThreadUtils;
    private Result<ConsistencyToken> mResult =
            Result.success(ConsistencyToken.getDefaultInstance());
    /*@Nullable*/ private Set<StreamUploadableAction> mActions;

    public FakeActionUploadRequestManager(FakeThreadUtils fakeThreadUtils) {
        this.mFakeThreadUtils = fakeThreadUtils;
    }

    @Override
    public void triggerUploadActions(Set<StreamUploadableAction> actions, ConsistencyToken token,
            Consumer<Result<ConsistencyToken>> consumer) {
        this.mActions = actions;
        boolean policy = mFakeThreadUtils.enforceMainThread(false);
        try {
            consumer.accept(mResult);
        } finally {
            mFakeThreadUtils.enforceMainThread(policy);
        }
    }

    /**
     * Sets the result to return from triggerUploadActions. If unset will use {@code
     * Result.success(ConsistencyToken.getDefaultInstance())}.
     */
    public FakeActionUploadRequestManager setResult(Result<ConsistencyToken> result) {
        this.mResult = result;
        return this;
    }

    /** Returns the last set of actions sent to triggerUploadActions. */
    public Set<StreamUploadableAction> getLatestActions() {
        if (mActions == null) {
            return new HashSet<>();
        }

        return mActions;
    }
}
