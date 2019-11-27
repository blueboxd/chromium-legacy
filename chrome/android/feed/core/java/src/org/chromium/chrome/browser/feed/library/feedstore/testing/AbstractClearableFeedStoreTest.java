// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.testing;

import static com.google.common.truth.Truth.assertThat;

import com.google.protobuf.ByteString;

import org.junit.Test;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation.ActionType;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.feedstore.internal.ClearableStore;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests of the {@link ClearableStore} classes. */
public abstract class AbstractClearableFeedStoreTest extends AbstractFeedStoreTest {
    protected static final String CONTENT_ID = "CONTENT_ID";
    protected static final String CONTENT_ID_2 = "CONTENT_ID_2";
    protected static final String SESSION_ID = "SESSION";
    protected static final String SESSION_ID_2 = "SESSION_2";
    protected static final StreamPayload STREAM_PAYLOAD =
            StreamPayload.newBuilder()
                    .setStreamFeature(StreamFeature.newBuilder().setContentId(CONTENT_ID))
                    .build();
    protected static final StreamPayload STREAM_PAYLOAD_2 =
            StreamPayload.newBuilder()
                    .setStreamFeature(StreamFeature.newBuilder().setContentId(CONTENT_ID_2))
                    .build();
    protected static final StreamPayload STREAM_PAYLOAD_SHARED_STATE =
            StreamPayload.newBuilder()
                    .setStreamSharedState(StreamSharedState.newBuilder().setContentId(CONTENT_ID))
                    .build();
    protected static final StreamPayload STREAM_PAYLOAD_SHARED_STATE_2 =
            StreamPayload.newBuilder()
                    .setStreamSharedState(StreamSharedState.newBuilder().setContentId(CONTENT_ID_2))
                    .build();
    protected static final byte[] SEMANTIC_PROPERTIES = new byte[] {14, 23, 54, 7};
    protected static final byte[] SEMANTIC_PROPERTIES_2 = new byte[] {12, 24, 18, 9};

    @Test
    public void clearAll() {
        ClearableStore store =
                (ClearableStore) getStore(FakeMainThreadRunner.runTasksImmediately());

        /*
        SETUP
        */

        // Payload
        CommitResult commitResult = store.editContent()
                                            .add(CONTENT_ID, STREAM_PAYLOAD)
                                            .add(CONTENT_ID_2, STREAM_PAYLOAD_2)
                                            .commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<List<PayloadWithId>> payloadsResult =
                store.getPayloads(Arrays.asList(CONTENT_ID, CONTENT_ID_2));
        assertThat(payloadsResult.isSuccessful()).isTrue();
        assertThat(payloadsResult.getValue()).hasSize(2);

        // Semantic properties
        commitResult = store.editSemanticProperties()
                               .add(CONTENT_ID, ByteString.copyFrom(SEMANTIC_PROPERTIES))
                               .add(CONTENT_ID_2, ByteString.copyFrom(SEMANTIC_PROPERTIES_2))
                               .commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<List<SemanticPropertiesWithId>> semanticPropertiesResult =
                store.getSemanticProperties(Arrays.asList(CONTENT_ID, CONTENT_ID_2));
        assertThat(semanticPropertiesResult.isSuccessful()).isTrue();
        assertThat(semanticPropertiesResult.getValue()).hasSize(2);

        // Shared State
        commitResult = store.editContent()
                               .add(CONTENT_ID, STREAM_PAYLOAD_SHARED_STATE)
                               .add(CONTENT_ID_2, STREAM_PAYLOAD_SHARED_STATE_2)
                               .commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<List<StreamSharedState>> sharedStatesResult = store.getSharedStates();
        assertThat(sharedStatesResult.isSuccessful()).isTrue();
        assertThat(sharedStatesResult.getValue()).hasSize(2);

        // Journal
        boolean boolCommitResult =
                store.editSession(SESSION_ID)
                        .add(StreamStructure.newBuilder().setContentId(CONTENT_ID).build())
                        .commit();
        assertThat(boolCommitResult).isTrue();
        boolCommitResult =
                store.editSession(SESSION_ID_2)
                        .add(StreamStructure.newBuilder().setContentId(CONTENT_ID_2).build())
                        .commit();
        assertThat(boolCommitResult).isTrue();

        Result<List<String>> sessionsResult = store.getAllSessions();
        assertThat(sessionsResult.isSuccessful()).isTrue();
        assertThat(sessionsResult.getValue()).hasSize(2);

        // Actions
        commitResult = store.editLocalActions()
                               .add(ActionType.DISMISS, CONTENT_ID)
                               .add(ActionType.DISMISS, CONTENT_ID_2)
                               .commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<List<StreamLocalAction>> dismissActionsResult = store.getAllDismissLocalActions();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        assertThat(dismissActionsResult.getValue()).hasSize(2);

        /*
        CLEAR
        */

        assertThat(store.clearAll()).isTrue();

        /*
        VERIFICATION
        */

        // Payload
        payloadsResult = store.getPayloads(Collections.singletonList(CONTENT_ID));
        assertThat(payloadsResult.isSuccessful()).isTrue();
        assertThat(payloadsResult.getValue()).hasSize(0);

        // Semantic properties (should not be cleared)
        semanticPropertiesResult =
                store.getSemanticProperties(Arrays.asList(CONTENT_ID, CONTENT_ID_2));
        assertThat(semanticPropertiesResult.isSuccessful()).isTrue();
        assertThat(semanticPropertiesResult.getValue()).hasSize(0);

        // Shared state
        sharedStatesResult = store.getSharedStates();
        assertThat(sharedStatesResult.isSuccessful()).isTrue();
        assertThat(sharedStatesResult.getValue()).hasSize(0);

        // Journal
        sessionsResult = store.getAllSessions();
        assertThat(sessionsResult.isSuccessful()).isTrue();
        assertThat(sessionsResult.getValue()).hasSize(0);

        // Actions (should not be cleared)
        dismissActionsResult = store.getAllDismissLocalActions();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        assertThat(dismissActionsResult.getValue()).hasSize(0);
    }
}
