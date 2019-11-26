// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.store;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.store.ContentMutation;
import com.google.android.libraries.feed.api.internal.store.SemanticPropertiesMutation;
import com.google.android.libraries.feed.api.internal.store.SessionMutation;
import com.google.android.libraries.feed.api.internal.store.UploadableActionMutation;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.feedstore.FeedStore;
import com.google.android.libraries.feed.hostimpl.storage.testing.InMemoryContentStorage;
import com.google.android.libraries.feed.hostimpl.storage.testing.InMemoryJournalStorage;
import com.google.android.libraries.feed.testing.host.logging.FakeBasicLoggingApi;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.wire.feed.SemanticPropertiesProto.SemanticProperties;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Fake implementation of {@link com.google.android.libraries.feed.api.internal.store.Store}. */
public final class FakeStore extends FeedStore {
    private final FakeThreadUtils mFakeThreadUtils;
    private boolean mAllowCreateNewSession = true;
    private boolean mAllowEditContent = true;
    private boolean mAllowGetPayloads = true;
    private boolean mAllowGetStreamStructures = true;
    private boolean mAllowGetSharedStates = true;
    private boolean mClearHeadCalled;

    public FakeStore(Configuration configuration, FakeThreadUtils fakeThreadUtils,
            TaskQueue taskQueue, Clock clock) {
        super(configuration, new TimingUtils(), new FeedExtensionRegistry(ArrayList::new),
                new InMemoryContentStorage(), new InMemoryJournalStorage(), fakeThreadUtils,
                taskQueue, clock, new FakeBasicLoggingApi(),
                FakeMainThreadRunner.runTasksImmediately());
        this.mFakeThreadUtils = fakeThreadUtils;
    }

    @Override
    public Result<List<PayloadWithId>> getPayloads(List<String> contentIds) {
        if (!mAllowGetPayloads) {
            return Result.failure();
        }

        return super.getPayloads(contentIds);
    }

    @Override
    public Result<List<StreamStructure>> getStreamStructures(String sessionId) {
        if (!mAllowGetStreamStructures) {
            return Result.failure();
        }

        return super.getStreamStructures(sessionId);
    }

    @Override
    public void clearHead() {
        mClearHeadCalled = true;
        super.clearHead();
    }

    @Override
    public Result<String> createNewSession() {
        if (!mAllowCreateNewSession) {
            return Result.failure();
        }

        return super.createNewSession();
    }

    @Override
    public Result<List<StreamSharedState>> getSharedStates() {
        if (!mAllowGetSharedStates) {
            return Result.failure();
        }

        return super.getSharedStates();
    }

    @Override
    public ContentMutation editContent() {
        if (!mAllowEditContent) {
            return new ContentMutation() {
                @Override
                public ContentMutation add(String contentId, StreamPayload payload) {
                    return this;
                }

                @Override
                public CommitResult commit() {
                    return CommitResult.FAILURE;
                }
            };
        }

        return super.editContent();
    }

    /** Returns if {@link FeedStore#clearAll()} was called. */
    public boolean getClearHeadCalled() {
        return mClearHeadCalled;
    }

    /** Clears all content storage. */
    public FakeStore clearContent() {
        mContentStorage.commit(
                new com.google.android.libraries.feed.api.host.storage.ContentMutation.Builder()
                        .deleteAll()
                        .build());
        return this;
    }

    /** Sets whether to fail on calls to {@link getStreamStructures(String)}. */
    public FakeStore setAllowGetStreamStructures(boolean value) {
        mAllowGetStreamStructures = value;
        return this;
    }

    /** Sets whether to fail on calls to {@link editContent()}. */
    public FakeStore setAllowEditContent(boolean value) {
        mAllowEditContent = value;
        return this;
    }

    /** Sets whether to fail on calls to {@link createNewSession()}. */
    public FakeStore setAllowCreateNewSession(boolean value) {
        mAllowCreateNewSession = value;
        return this;
    }

    /** Sets whether to fail on calls to {@link getPayloads(List)}. */
    public FakeStore setAllowGetPayloads(boolean value) {
        mAllowGetPayloads = value;
        return this;
    }

    /** Sets whether to fail on calls to {@link getSharedStates()}. */
    public FakeStore setAllowGetSharedStates(boolean value) {
        mAllowGetSharedStates = value;
        return this;
    }

    /** Adds the {@code payload} to the store. */
    public FakeStore setContent(String contentId, StreamPayload payload) {
        boolean policy = mFakeThreadUtils.enforceMainThread(false);
        editContent().add(contentId, payload).commit();
        mFakeThreadUtils.enforceMainThread(policy);
        return this;
    }

    /** Adds the {@code payloads} to the store. */
    public FakeStore setContent(List<PayloadWithId> payloads) {
        boolean policy = mFakeThreadUtils.enforceMainThread(false);
        ContentMutation mutation = editContent();
        for (PayloadWithId payload : payloads) {
            mutation.add(payload.contentId, payload.payload);
        }
        mutation.commit();
        mFakeThreadUtils.enforceMainThread(policy);
        return this;
    }

    /** Adds the {@code sharedStates} to the store. */
    public FakeStore setSharedStates(StreamSharedState... sharedStates) {
        boolean policy = mFakeThreadUtils.enforceMainThread(false);
        ContentMutation mutation = editContent();
        for (StreamSharedState sharedState : sharedStates) {
            mutation.add(sharedState.getContentId(),
                    StreamPayload.newBuilder().setStreamSharedState(sharedState).build());
        }
        mutation.commit();
        mFakeThreadUtils.enforceMainThread(policy);
        return this;
    }

    /** Adds the {@code structures} to the store for the specified {@code sessionId}. */
    public FakeStore setStreamStructures(String sessionId, StreamStructure... structures) {
        return setStreamStructures(sessionId, Arrays.asList(structures));
    }

    /** Adds the {@code structures} to the store for the specified {@code sessionId}. */
    public FakeStore setStreamStructures(String sessionId, List<StreamStructure> structures) {
        boolean policy = mFakeThreadUtils.enforceMainThread(false);
        SessionMutation mutation = editSession(sessionId);
        for (StreamStructure structure : structures) {
            mutation.add(structure);
        }
        mutation.commit();
        mFakeThreadUtils.enforceMainThread(policy);
        return this;
    }

    /** Adds the {@code actions} to the store. */
    public FakeStore setStreamUploadableActions(StreamUploadableAction... actions) {
        boolean policy = mFakeThreadUtils.enforceMainThread(false);
        UploadableActionMutation mutation = editUploadableActions();
        for (StreamUploadableAction action : actions) {
            mutation.upsert(action, action.getFeatureContentId());
        }
        mutation.commit();
        mFakeThreadUtils.enforceMainThread(policy);
        return this;
    }

    /** Adds the {@code actions} to the store. */
    public FakeStore addSemanticProperties(String contentId, SemanticProperties properties) {
        boolean policy = mFakeThreadUtils.enforceMainThread(false);
        SemanticPropertiesMutation mutation = editSemanticProperties();
        mutation.add(contentId, properties.getSemanticPropertiesData());
        mutation.commit();
        mFakeThreadUtils.enforceMainThread(policy);
        return this;
    }

    /** Gets all content associated with the {@code contentId}. */
    public List<Object> getContentById(String contentId) {
        boolean policy = mFakeThreadUtils.enforceMainThread(false);
        ArrayList<String> contentIds = new ArrayList<>(1);
        contentIds.add(contentId);
        ArrayList<Object> result = new ArrayList<>();
        result.addAll(getPayloads(contentIds).getValue());
        result.addAll(getSemanticProperties(contentIds).getValue());
        for (StreamUploadableAction action : getAllUploadableActions().getValue()) {
            if (action.getFeatureContentId().equals(contentId)) {
                result.add(action);
            }
        }
        mFakeThreadUtils.enforceMainThread(policy);
        return result;
    }
}
