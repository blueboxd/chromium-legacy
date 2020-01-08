// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.testing;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.api.internal.store.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.SemanticPropertiesMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.SessionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.api.internal.store.StoreListener;
import org.chromium.chrome.browser.feed.library.api.internal.store.UploadableActionMutation;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;

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
