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

package com.google.android.libraries.feed.feedsessionmanager.internal.testing;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.common.testing.InternalProtocolBuilder;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.store.SessionMutation;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.feedsessionmanager.internal.SessionImpl;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelMutation;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelProvider;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import java.util.ArrayList;
import java.util.List;
import org.junit.Test;
import org.mockito.Mock;

/** Abstract class implementing many of the core SessionImpl tests. */
public abstract class AbstractSessionImplTest {
  protected static final String TEST_SESSION_ID = "TEST$1";
  protected static final int SCHEMA_VERSION = 1;

  protected final ContentIdGenerators contentIdGenerators = new ContentIdGenerators();

  @Mock protected Store store;
  protected FakeSessionMutation fakeSessionMutation;
  protected FakeModelProvider fakeModelProvider;

  private Boolean sessionMutationResults = true;

  protected void setUp() {
    initMocks(this);
    fakeSessionMutation = new FakeSessionMutation();
    fakeModelProvider = new FakeModelProvider();
    when(store.editSession(TEST_SESSION_ID)).thenReturn(fakeSessionMutation);
  }

  protected abstract SessionImpl getSessionImpl();

  @Test
  public void testPopulateModelProvider_features() {
    SessionImpl session = getSessionImpl();
    int featureCnt = 3;
    InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
    addFeatures(protocolBuilder, featureCnt, 1);
    List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

    // 3 features
    assertThat(streamStructures).hasSize(featureCnt);
    session.setSessionId(TEST_SESSION_ID);
    session.populateModelProvider(streamStructures, false, false, UiContext.getDefaultInstance());
    FakeModelMutation fakeModelMutation = fakeModelProvider.getLatestModelMutation();
    assertThat(fakeModelMutation.addedChildren).hasSize(featureCnt);
    assertThat(fakeModelMutation.isCommitted()).isTrue();
    assertThat(session.getContentInSession()).hasSize(featureCnt);
    assertThat(session.getContentInSession())
        .contains(contentIdGenerators.createFeatureContentId(1));
  }

  @Test
  public void testUpdateSession_notFullyInitialized() {
    SessionImpl session = getSessionImpl();
    assertThatRunnable(() -> session.updateSession(false, new ArrayList<>(), SCHEMA_VERSION, null))
        .throwsAnExceptionOfType(NullPointerException.class);
  }

  @Test
  public void testUpdateSession_features() {
    SessionImpl session = getSessionImpl();
    session.setSessionId(TEST_SESSION_ID);
    session.populateModelProvider(new ArrayList<>(), false, false, UiContext.getDefaultInstance());
    int featureCnt = 3;
    InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder().addClearOperation();
    addFeatures(protocolBuilder, featureCnt, 1);
    List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

    // 1 clear, 3 features
    assertThat(streamStructures).hasSize(4);
    session.updateSession(false, streamStructures, SCHEMA_VERSION, null);
    assertThat(fakeSessionMutation.streamStructures).hasSize(featureCnt);
    FakeModelMutation fakeModelMutation = fakeModelProvider.getLatestModelMutation();
    assertThat(fakeModelMutation.addedChildren).hasSize(featureCnt);
    assertThat(session.getContentInSession()).hasSize(featureCnt);
    assertThat(session.getContentInSession())
        .contains(contentIdGenerators.createFeatureContentId(1));
  }

  @Test
  public void testUpdateFromToken() {
    SessionImpl session = getSessionImpl();
    session.setSessionId(TEST_SESSION_ID);
    session.populateModelProvider(new ArrayList<>(), false, false, UiContext.getDefaultInstance());
    int featureCnt = 3;
    InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
    addFeatures(protocolBuilder, featureCnt, 1);
    List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

    StreamToken token =
        StreamToken.newBuilder().setContentId(contentIdGenerators.createTokenContentId(2)).build();
    MutationContext context = new MutationContext.Builder().setContinuationToken(token).build();

    // The token needs to be in the session so temporarily detach the session to update its content
    // IDs with the token.
    ModelProvider modelProvider = session.getModelProvider();
    session.bindModelProvider(null, null);

    List<StreamStructure> tokenStructures =
        new InternalProtocolBuilder().addToken(token.getContentId()).buildAsStreamStructure();
    session.updateSession(false, tokenStructures, SCHEMA_VERSION, null);

    session.bindModelProvider(modelProvider, null);

    session.updateSession(false, streamStructures, SCHEMA_VERSION, context);
    FakeModelMutation fakeModelMutation = fakeModelProvider.getLatestModelMutation();
    assertThat(fakeModelMutation.mutationContext).isEqualTo(context);
  }

  @Test
  public void testUpdateFromToken_notInSession() {
    SessionImpl session = getSessionImpl();
    session.setSessionId(TEST_SESSION_ID);
    session.populateModelProvider(new ArrayList<>(), false, false, UiContext.getDefaultInstance());
    int featureCnt = 3;
    InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
    addFeatures(protocolBuilder, featureCnt, 1);
    List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

    // The token is not in the session, so we ignore the update
    assertThat(session.getContentInSession()).isEmpty();
    StreamToken token =
        StreamToken.newBuilder().setContentId(contentIdGenerators.createTokenContentId(2)).build();
    MutationContext context = new MutationContext.Builder().setContinuationToken(token).build();
    session.updateSession(false, streamStructures, SCHEMA_VERSION, context);
    assertThat(session.getContentInSession()).isEmpty();
  }

  @Test
  public void testStorageFailure() {
    SessionImpl session = getSessionImpl();
    session.setSessionId(TEST_SESSION_ID);
    session.populateModelProvider(new ArrayList<>(), false, false, UiContext.getDefaultInstance());
    int featureCnt = 3;
    InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
    addFeatures(protocolBuilder, featureCnt, 1);
    List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

    sessionMutationResults = false;
    session.updateSession(false, streamStructures, SCHEMA_VERSION, null);
    FakeModelMutation fakeModelMutation = fakeModelProvider.getLatestModelMutation();
    assertThat(fakeModelMutation.isCommitted()).isTrue(); // Optimistic write will still call this
  }

  @Test
  public void testRemove() {
    SessionImpl session = getSessionImpl();
    session.setSessionId(TEST_SESSION_ID);
    session.populateModelProvider(new ArrayList<>(), false, false, UiContext.getDefaultInstance());
    InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
    int featureCnt = 2;
    addFeatures(protocolBuilder, featureCnt, 1);
    protocolBuilder.removeFeature(
        contentIdGenerators.createFeatureContentId(1), contentIdGenerators.createRootContentId(0));
    List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();
    session.updateSession(false, streamStructures, SCHEMA_VERSION, null);
    FakeModelMutation fakeModelMutation = fakeModelProvider.getLatestModelMutation();
    assertThat(fakeModelMutation.removedChildren).hasSize(1);
    assertThat(session.getContentInSession()).hasSize(1);
  }

  protected void addFeatures(InternalProtocolBuilder protocolBuilder, int featureCnt, int startId) {
    for (int i = 0; i < featureCnt; i++) {
      protocolBuilder.addFeature(
          contentIdGenerators.createFeatureContentId(startId++),
          contentIdGenerators.createRootContentId(0));
    }
  }

  /** Fake SessionMutation for tests. */
  protected final class FakeSessionMutation implements SessionMutation {

    public final List<StreamStructure> streamStructures = new ArrayList<>();

    @Override
    public SessionMutation add(StreamStructure dataOperation) {
      streamStructures.add(dataOperation);
      return this;
    }

    @Override
    public Boolean commit() {
      return sessionMutationResults;
    }
  }
}
