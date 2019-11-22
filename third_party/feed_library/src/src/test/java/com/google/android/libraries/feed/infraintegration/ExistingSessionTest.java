// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.State;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests which verify creating a new Model Provider from an existing session. */
@RunWith(RobolectricTestRunner.class)
public class ExistingSessionTest {
    private static final ContentId[] CARDS = new ContentId[] {
            ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2),
            ResponseBuilder.createFeatureContentId(3)};

    private final InfraIntegrationScope scope = new InfraIntegrationScope.Builder().build();
    private final ModelProviderFactory modelProviderFactory = scope.getModelProviderFactory();
    private final ModelProviderValidator modelValidator =
            new ModelProviderValidator(scope.getProtocolAdapter());

    @Before
    public void setUp() {
        // Create a simple stream with a root and three features
        scope.getFakeFeedRequestManager()
                .queueResponse(ResponseBuilder.forClearAllWithCards(CARDS).build())
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        scope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));
    }

    @After
    public void tearDown() {
        assertThat(scope.getTaskQueue().hasBacklog()).isFalse();
        assertThat(scope.getFakeMainThreadRunner().hasPendingTasks()).isFalse();
    }

    @Test
    public void createModelProvider() {
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        ModelFeature initRoot = modelProvider.getRootFeature();
        assertThat(initRoot).isNotNull();
        ModelCursor initCursor = initRoot.getCursor();
        assertThat(initCursor).isNotNull();

        String sessionId = modelProvider.getSessionId();
        assertThat(sessionId).isNotEmpty();

        ModelProvider modelProvider2 =
                modelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        assertThat(modelProvider2).isNotNull();
        ModelFeature root2 = modelProvider2.getRootFeature();
        assertThat(root2).isNotNull();
        ModelCursor cursor2 = root2.getCursor();
        assertThat(cursor2).isNotNull();
        assertThat(modelProvider2.getSessionId()).isEqualTo(sessionId);
        modelValidator.assertCursorContents(cursor2, CARDS);

        // Creating the new session will invalidate the previous ModelProvider and Cursor
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INVALIDATED);
        assertThat(initCursor.isAtEnd()).isTrue();
    }

    @Test
    public void createModelProvider_restoreSession() {
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        String sessionId = modelProvider.getSessionId();
        modelProvider.detachModelProvider();

        // Restore the session.
        ModelProvider restoredModelProvider =
                modelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        assertThat(restoredModelProvider).isNotNull();
        assertThat(restoredModelProvider.getAllRootChildren()).hasSize(CARDS.length);
    }

    @Test
    public void createModelProvider_restoreSessionWithOutstandingRequest() {
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        String sessionId = modelProvider.getSessionId();
        modelProvider.detachModelProvider();

        // Start a request.
        scope.getFakeFeedRequestManager()
                .queueResponse(
                        ResponseBuilder.forClearAllWithCards(CARDS).build(), /* delayMs= */ 100)
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        scope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));

        // Restore the session.
        ModelProvider restoredModelProvider =
                modelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        assertThat(restoredModelProvider.getAllRootChildren()).hasSize(CARDS.length);

        // Advance the clock to process the outstanding request.
        scope.getFakeClock().advance(100);
    }

    @Test
    public void createModelProvider_unknownSession() {
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        ModelFeature initRoot = modelProvider.getRootFeature();
        assertThat(initRoot).isNotNull();
        ModelCursor initCursor = initRoot.getCursor();
        assertThat(initCursor).isNotNull();

        String sessionId = modelProvider.getSessionId();
        assertThat(sessionId).isNotEmpty();

        // Create a second model provider using an unknown session token, this should return null
        ModelProvider modelProvider2 =
                modelProviderFactory.create("unknown-session", UiContext.getDefaultInstance());
        assertThat(modelProvider2).isNotNull();
        assertThat(modelProvider2.getCurrentState()).isEqualTo(State.INVALIDATED);

        // Now create one from head
        modelProvider2 = modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider2).isNotNull();
        ModelFeature root2 = modelProvider2.getRootFeature();
        assertThat(root2).isNotNull();
        ModelCursor cursor2 = root2.getCursor();
        assertThat(cursor2).isNotNull();
        modelValidator.assertCursorContents(cursor2, CARDS);

        // two sessions against the same $HEAD instance
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        assertThat(modelProvider2.getCurrentState()).isEqualTo(State.READY);
    }

    @Test
    public void createModelProvider_invalidatedSession() {
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider).isNotNull();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        String sessionId = modelProvider.getSessionId();
        assertThat(sessionId).isNotEmpty();

        modelProvider.invalidate();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INVALIDATED);

        // Attempt to connect with the session token we just invalidated.  This will result in a
        // INVALIDATED ModelProvider.
        ModelProvider modelProvider2 =
                modelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        assertThat(modelProvider2).isNotNull();
        assertThat(modelProvider2.getCurrentState()).isEqualTo(State.INVALIDATED);
    }

    @Test
    public void createModelProvider_detachSession() {
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider).isNotNull();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        String sessionId = modelProvider.getSessionId();
        assertThat(sessionId).isNotEmpty();

        modelProvider.detachModelProvider();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INVALIDATED);

        // Attempt to connect with the session token we just detached.  This will work as expected.
        ModelProvider modelProvider2 =
                modelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        assertThat(modelProvider2).isNotNull();
        assertThat(modelProvider2.getCurrentState()).isEqualTo(State.READY);
    }
}
