// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider.internal;

import android.support.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelMutation;
import com.google.android.libraries.feed.common.functional.Committer;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;

import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of the {@link ModelMutation}. When the mutation is committed updating the Model
 */
public final class ModelMutationImpl implements ModelMutation {
    /** The data representing the Change to the model. */
    public static final class Change {
        public final List<StreamStructure> structureChanges = new ArrayList<>();
        public final List<StreamStructure> updateChanges = new ArrayList<>();
        /*@Nullable*/ public MutationContext mutationContext;
        /*@Nullable*/ public String sessionId;
        public boolean cachedBindings;
    }

    private final Committer<Void, Change> committer;
    @VisibleForTesting
    final Change change = new Change();

    public ModelMutationImpl(Committer<Void, Change> committer) {
        this.committer = committer;
    }

    @Override
    public ModelMutation addChild(StreamStructure streamStructure) {
        change.structureChanges.add(streamStructure);
        return this;
    }

    @Override
    public ModelMutation removeChild(StreamStructure streamStructure) {
        change.structureChanges.add(streamStructure);
        return this;
    }

    @Override
    public ModelMutation updateChild(StreamStructure updateChild) {
        change.updateChanges.add(updateChild);
        return this;
    }

    @Override
    public ModelMutation setMutationContext(MutationContext mutationContext) {
        change.mutationContext = mutationContext;
        return this;
    }

    @Override
    public ModelMutation setSessionId(String sessionId) {
        change.sessionId = sessionId;
        return this;
    }

    @Override
    public ModelMutation hasCachedBindings(boolean cachedBindings) {
        change.cachedBindings = cachedBindings;
        return this;
    }

    @Override
    public void commit() {
        committer.commit(change);
    }
}
