// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider.internal;

import android.support.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelMutation;
import org.chromium.chrome.browser.feed.library.common.functional.Committer;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;

import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of the {@link ModelMutation}. When the mutation is committed updating the Model
 */
public final class ModelMutationImpl implements ModelMutation {
    /** The data representing the Change to the model. */
    public static final class Change {
        public final List<StreamStructure> mStructureChanges = new ArrayList<>();
        public final List<StreamStructure> mUpdateChanges = new ArrayList<>();
        /*@Nullable*/ public MutationContext mMutationContext;
        /*@Nullable*/ public String mSessionId;
        public boolean mCachedBindings;
    }

    private final Committer<Void, Change> mCommitter;
    @VisibleForTesting
    final Change mChange = new Change();

    public ModelMutationImpl(Committer<Void, Change> committer) {
        this.mCommitter = committer;
    }

    @Override
    public ModelMutation addChild(StreamStructure streamStructure) {
        mChange.mStructureChanges.add(streamStructure);
        return this;
    }

    @Override
    public ModelMutation removeChild(StreamStructure streamStructure) {
        mChange.mStructureChanges.add(streamStructure);
        return this;
    }

    @Override
    public ModelMutation updateChild(StreamStructure updateChild) {
        mChange.mUpdateChanges.add(updateChild);
        return this;
    }

    @Override
    public ModelMutation setMutationContext(MutationContext mutationContext) {
        mChange.mMutationContext = mutationContext;
        return this;
    }

    @Override
    public ModelMutation setSessionId(String sessionId) {
        mChange.mSessionId = sessionId;
        return this;
    }

    @Override
    public ModelMutation hasCachedBindings(boolean cachedBindings) {
        mChange.mCachedBindings = cachedBindings;
        return this;
    }

    @Override
    public void commit() {
        mCommitter.commit(mChange);
    }
}
