// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.testing;

import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.common.SemanticPropertiesWithId;
import com.google.android.libraries.feed.api.internal.store.ContentMutation;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation;
import com.google.android.libraries.feed.api.internal.store.SemanticPropertiesMutation;
import com.google.android.libraries.feed.api.internal.store.SessionMutation;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.api.internal.store.StoreListener;
import com.google.android.libraries.feed.api.internal.store.UploadableActionMutation;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.search.now.feed.client.StreamDataProto.StreamLocalAction;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;

import java.util.List;
import java.util.Set;

/** Class which allows spying on a Store implementation */
public class DelegatingStore implements Store {
    private final Store mStore;

    public DelegatingStore(Store store) {
        this.mStore = store;
    }

    @Override
    public Result<List<PayloadWithId>> getPayloads(List<String> contentIds) {
        return mStore.getPayloads(contentIds);
    }

    @Override
    public Result<List<StreamSharedState>> getSharedStates() {
        return mStore.getSharedStates();
    }

    @Override
    public Result<List<StreamStructure>> getStreamStructures(String sessionId) {
        return mStore.getStreamStructures(sessionId);
    }

    @Override
    public Result<List<String>> getAllSessions() {
        return mStore.getAllSessions();
    }

    @Override
    public Result<List<SemanticPropertiesWithId>> getSemanticProperties(List<String> contentIds) {
        return mStore.getSemanticProperties(contentIds);
    }

    @Override
    public Result<List<StreamLocalAction>> getAllDismissLocalActions() {
        return mStore.getAllDismissLocalActions();
    }

    @Override
    public Result<Set<StreamUploadableAction>> getAllUploadableActions() {
        return mStore.getAllUploadableActions();
    }

    @Override
    public Result<String> createNewSession() {
        return mStore.createNewSession();
    }

    @Override
    public void removeSession(String sessionId) {
        mStore.removeSession(sessionId);
    }

    @Override
    public void clearHead() {
        mStore.clearHead();
    }

    @Override
    public ContentMutation editContent() {
        return mStore.editContent();
    }

    @Override
    public SessionMutation editSession(String sessionId) {
        return mStore.editSession(sessionId);
    }

    @Override
    public SemanticPropertiesMutation editSemanticProperties() {
        return mStore.editSemanticProperties();
    }

    @Override
    public LocalActionMutation editLocalActions() {
        return mStore.editLocalActions();
    }

    @Override
    public UploadableActionMutation editUploadableActions() {
        return mStore.editUploadableActions();
    }

    @Override
    public Runnable triggerContentGc(Set<String> reservedContentIds,
            Supplier<Set<String>> accessibleContent, boolean keepSharedStates) {
        return mStore.triggerContentGc(reservedContentIds, accessibleContent, keepSharedStates);
    }

    @Override
    public Runnable triggerLocalActionGc(
            List<StreamLocalAction> actions, List<String> validContentIds) {
        return mStore.triggerLocalActionGc(actions, validContentIds);
    }

    @Override
    public void switchToEphemeralMode() {
        mStore.switchToEphemeralMode();
    }

    @Override
    public boolean isEphemeralMode() {
        return mStore.isEphemeralMode();
    }

    @Override
    public void registerObserver(StoreListener observer) {
        mStore.registerObserver(observer);
    }

    @Override
    public void unregisterObserver(StoreListener observer) {
        mStore.unregisterObserver(observer);
    }
}
