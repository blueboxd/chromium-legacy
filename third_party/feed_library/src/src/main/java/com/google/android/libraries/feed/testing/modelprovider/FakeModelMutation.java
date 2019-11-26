// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.modelprovider;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelMutation;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;

import java.util.ArrayList;
import java.util.List;

/** Fake ModelMutation for tests. */
public final class FakeModelMutation implements ModelMutation {
    public final List<StreamStructure> mAddedChildren = new ArrayList<>();
    public final List<StreamStructure> mRemovedChildren = new ArrayList<>();
    public final List<StreamStructure> mUpdateChildren = new ArrayList<>();
    public MutationContext mMutationContext;
    boolean mCommitCalled;

    @Override
    public ModelMutation addChild(StreamStructure streamStructure) {
        mAddedChildren.add(streamStructure);
        return this;
    }

    @Override
    public ModelMutation removeChild(StreamStructure streamStructure) {
        mRemovedChildren.add(streamStructure);
        return this;
    }

    @Override
    public ModelMutation updateChild(StreamStructure streamStructure) {
        mUpdateChildren.add(streamStructure);
        return this;
    }

    @Override
    public ModelMutation setMutationContext(MutationContext mutationContext) {
        this.mMutationContext = mutationContext;
        return this;
    }

    @Override
    public ModelMutation setSessionId(String sessionId) {
        return this;
    }

    @Override
    public ModelMutation hasCachedBindings(boolean cachedBindings) {
        return this;
    }

    @Override
    public void commit() {
        mCommitCalled = true;
    }

    /** Clears the commit. */
    public FakeModelMutation clearCommit() {
        mCommitCalled = false;
        mAddedChildren.clear();
        mRemovedChildren.clear();
        mUpdateChildren.clear();
        return this;
    }

    /** Returns whether this mutation was committed. */
    public boolean isCommitted() {
        return mCommitCalled;
    }
}
