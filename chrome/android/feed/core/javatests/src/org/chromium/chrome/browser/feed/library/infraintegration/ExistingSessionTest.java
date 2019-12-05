// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.State;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests which verify creating a new Model Provider from an existing session. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExistingSessionTest {
    private static final ContentId[] CARDS = new ContentId[] {
            ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2),
            ResponseBuilder.createFeatureContentId(3)};

    private final InfraIntegrationScope mScope = new InfraIntegrationScope.Builder().build();
    private final ModelProviderFactory mModelProviderFactory = mScope.getModelProviderFactory();
    private final ModelProviderValidator mModelValidator =
            new ModelProviderValidator(mScope.getProtocolAdapter());

    @Before
    public void setUp() {
        // Create a simple stream with a root and three features
        mScope.getFakeFeedRequestManager()
                .queueResponse(ResponseBuilder.forClearAllWithCards(CARDS).build())
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        mScope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));
    }

    @After
    public void tearDown() {
        assertThat(mScope.getTaskQueue().hasBacklog()).isFalse();
        assertThat(mScope.getFakeMainThreadRunner().hasPendingTasks()).isFalse();
    }

    @Test
    public void createModelProvider() {
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        ModelFeature initRoot = modelProvider.getRootFeature();
        assertThat(initRoot).isNotNull();
        ModelCursor initCursor = initRoot.getCursor();
        assertThat(initCursor).isNotNull();

        String sessionId = modelProvider.getSessionId();
        assertThat(sessionId).isNotEmpty();

        ModelProvider modelProvider2 =
                mModelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        assertThat(modelProvider2).isNotNull();
        ModelFeature root2 = modelProvider2.getRootFeature();
        assertThat(root2).isNotNull();
        ModelCursor cursor2 = root2.getCursor();
        assertThat(cursor2).isNotNull();
        assertThat(modelProvider2.getSessionId()).isEqualTo(sessionId);
        mModelValidator.assertCursorContents(cursor2, CARDS);

        // Creating the new session will invalidate the previous ModelProvider and Cursor
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INVALIDATED);
        assertThat(initCursor.isAtEnd()).isTrue();
    }

    @Test
    public void createModelProvider_restoreSession() {
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        String sessionId = modelProvider.getSessionId();
        modelProvider.detachModelProvider();

        // Restore the session.
        ModelProvider restoredModelProvider =
                mModelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        assertThat(restoredModelProvider).isNotNull();
        assertThat(restoredModelProvider.getAllRootChildren()).hasSize(CARDS.length);
    }

    @Test
    public void createModelProvider_restoreSessionWithOutstandingRequest() {
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        String sessionId = modelProvider.getSessionId();
        modelProvider.detachModelProvider();

        // Start a request.
        mScope.getFakeFeedRequestManager()
                .queueResponse(
                        ResponseBuilder.forClearAllWithCards(CARDS).build(), /* delayMs= */ 100)
                .triggerRefresh(RequestReason.OPEN_WITHOUT_CONTENT,
                        mScope.getFeedSessionManager().getUpdateConsumer(
                                MutationContext.EMPTY_CONTEXT));

        // Restore the session.
        ModelProvider restoredModelProvider =
                mModelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        assertThat(restoredModelProvider.getAllRootChildren()).hasSize(CARDS.length);

        // Advance the clock to process the outstanding request.
        mScope.getFakeClock().advance(100);
    }

    @Test
    public void createModelProvider_unknownSession() {
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        ModelFeature initRoot = modelProvider.getRootFeature();
        assertThat(initRoot).isNotNull();
        ModelCursor initCursor = initRoot.getCursor();
        assertThat(initCursor).isNotNull();

        String sessionId = modelProvider.getSessionId();
        assertThat(sessionId).isNotEmpty();

        // Create a second model provider using an unknown session token, this should return null
        ModelProvider modelProvider2 =
                mModelProviderFactory.create("unknown-session", UiContext.getDefaultInstance());
        assertThat(modelProvider2).isNotNull();
        assertThat(modelProvider2.getCurrentState()).isEqualTo(State.INVALIDATED);

        // Now create one from head
        modelProvider2 = mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider2).isNotNull();
        ModelFeature root2 = modelProvider2.getRootFeature();
        assertThat(root2).isNotNull();
        ModelCursor cursor2 = root2.getCursor();
        assertThat(cursor2).isNotNull();
        mModelValidator.assertCursorContents(cursor2, CARDS);

        // two sessions against the same $HEAD instance
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        assertThat(modelProvider2.getCurrentState()).isEqualTo(State.READY);
    }

    @Test
    public void createModelProvider_invalidatedSession() {
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider).isNotNull();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        String sessionId = modelProvider.getSessionId();
        assertThat(sessionId).isNotEmpty();

        modelProvider.invalidate();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INVALIDATED);

        // Attempt to connect with the session token we just invalidated.  This will result in a
        // INVALIDATED ModelProvider.
        ModelProvider modelProvider2 =
                mModelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        assertThat(modelProvider2).isNotNull();
        assertThat(modelProvider2.getCurrentState()).isEqualTo(State.INVALIDATED);
    }

    @Test
    public void createModelProvider_detachSession() {
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertThat(modelProvider).isNotNull();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        String sessionId = modelProvider.getSessionId();
        assertThat(sessionId).isNotEmpty();

        modelProvider.detachModelProvider();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INVALIDATED);

        // Attempt to connect with the session token we just detached.  This will work as expected.
        ModelProvider modelProvider2 =
                mModelProviderFactory.create(sessionId, UiContext.getDefaultInstance());
        assertThat(modelProvider2).isNotNull();
        assertThat(modelProvider2.getCurrentState()).isEqualTo(State.READY);
    }
}
