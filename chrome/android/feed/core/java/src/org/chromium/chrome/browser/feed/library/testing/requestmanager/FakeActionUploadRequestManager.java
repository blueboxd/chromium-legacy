// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.requestmanager;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.internal.requestmanager.ActionUploadRequestManager;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;

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
