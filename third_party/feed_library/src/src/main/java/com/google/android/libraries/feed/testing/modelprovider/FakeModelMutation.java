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
    public final List<StreamStructure> addedChildren = new ArrayList<>();
    public final List<StreamStructure> removedChildren = new ArrayList<>();
    public final List<StreamStructure> updateChildren = new ArrayList<>();
    public MutationContext mutationContext;
    boolean commitCalled;

    @Override
    public ModelMutation addChild(StreamStructure streamStructure) {
        addedChildren.add(streamStructure);
        return this;
    }

    @Override
    public ModelMutation removeChild(StreamStructure streamStructure) {
        removedChildren.add(streamStructure);
        return this;
    }

    @Override
    public ModelMutation updateChild(StreamStructure streamStructure) {
        updateChildren.add(streamStructure);
        return this;
    }

    @Override
    public ModelMutation setMutationContext(MutationContext mutationContext) {
        this.mutationContext = mutationContext;
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
        commitCalled = true;
    }

    /** Clears the commit. */
    public FakeModelMutation clearCommit() {
        commitCalled = false;
        addedChildren.clear();
        removedChildren.clear();
        updateChildren.clear();
        return this;
    }

    /** Returns whether this mutation was committed. */
    public boolean isCommitted() {
        return commitCalled;
    }
}
