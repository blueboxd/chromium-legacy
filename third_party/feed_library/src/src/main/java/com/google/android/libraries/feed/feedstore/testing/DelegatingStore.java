// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
  private final Store store;

  public DelegatingStore(Store store) {
    this.store = store;
  }

  @Override
  public Result<List<PayloadWithId>> getPayloads(List<String> contentIds) {
    return store.getPayloads(contentIds);
  }

  @Override
  public Result<List<StreamSharedState>> getSharedStates() {
    return store.getSharedStates();
  }

  @Override
  public Result<List<StreamStructure>> getStreamStructures(String sessionId) {
    return store.getStreamStructures(sessionId);
  }

  @Override
  public Result<List<String>> getAllSessions() {
    return store.getAllSessions();
  }

  @Override
  public Result<List<SemanticPropertiesWithId>> getSemanticProperties(List<String> contentIds) {
    return store.getSemanticProperties(contentIds);
  }

  @Override
  public Result<List<StreamLocalAction>> getAllDismissLocalActions() {
    return store.getAllDismissLocalActions();
  }

  @Override
  public Result<Set<StreamUploadableAction>> getAllUploadableActions() {
    return store.getAllUploadableActions();
  }

  @Override
  public Result<String> createNewSession() {
    return store.createNewSession();
  }

  @Override
  public void removeSession(String sessionId) {
    store.removeSession(sessionId);
  }

  @Override
  public void clearHead() {
    store.clearHead();
  }

  @Override
  public ContentMutation editContent() {
    return store.editContent();
  }

  @Override
  public SessionMutation editSession(String sessionId) {
    return store.editSession(sessionId);
  }

  @Override
  public SemanticPropertiesMutation editSemanticProperties() {
    return store.editSemanticProperties();
  }

  @Override
  public LocalActionMutation editLocalActions() {
    return store.editLocalActions();
  }

  @Override
  public UploadableActionMutation editUploadableActions() {
    return store.editUploadableActions();
  }

  @Override
  public Runnable triggerContentGc(
      Set<String> reservedContentIds,
      Supplier<Set<String>> accessibleContent,
      boolean keepSharedStates) {
    return store.triggerContentGc(reservedContentIds, accessibleContent, keepSharedStates);
  }

  @Override
  public Runnable triggerLocalActionGc(
      List<StreamLocalAction> actions, List<String> validContentIds) {
    return store.triggerLocalActionGc(actions, validContentIds);
  }

  @Override
  public void switchToEphemeralMode() {
    store.switchToEphemeralMode();
  }

  @Override
  public boolean isEphemeralMode() {
    return store.isEphemeralMode();
  }

  @Override
  public void registerObserver(StoreListener observer) {
    store.registerObserver(observer);
  }

  @Override
  public void unregisterObserver(StoreListener observer) {
    store.unregisterObserver(observer);
  }
}
