// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.android.libraries.feed.common.testing.ResponseBuilder.ROOT_CONTENT_ID;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi.RequestBehavior;
import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChange.ChildChanges;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderObserver;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.testing.host.scheduler.FakeSchedulerApi;
import com.google.android.libraries.feed.testing.requestmanager.FakeFeedRequestManager;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This is a TimeoutSession test which verifies REQUEST_WITH_CONTENT
 *
 * <p>NOTE: This test has multiple threads running. There is a single threaded executor created in
 * addition to the main thread. The Test will throw a TimeoutException in in production in the event
 * of a deadlock. The DEBUG boolean controls the behavior to allow debugging.
 */
@RunWith(RobolectricTestRunner.class)
public class TimeoutSessionWithContentTest {
    // This flag will should be flipped to debug the test.  It will disable TimeoutExceptions.
    private static final boolean DEBUG;
    private static final ContentId[] REQUEST_ONE = new ContentId[] {
            ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2),
            ResponseBuilder.createFeatureContentId(3)};
    private static final ContentId[] REQUEST_TWO = new ContentId[] {
            ResponseBuilder.createFeatureContentId(4), ResponseBuilder.createFeatureContentId(3),
            ResponseBuilder.createFeatureContentId(5)};

    private final FakeSchedulerApi fakeSchedulerApi =
            new FakeSchedulerApi(FakeThreadUtils.withoutThreadChecks());

    private FakeClock fakeClock;
    private FakeFeedRequestManager fakeFeedRequestManager;
    private ModelProviderFactory modelProviderFactory;
    private ProtocolAdapter protocolAdapter;
    private ModelProviderValidator modelValidator;
    private long timeoutDeadline;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder()
                                              .setSchedulerApi(fakeSchedulerApi)
                                              .withTimeoutSessionConfiguration(2L)
                                              .build();
        fakeClock = scope.getFakeClock();
        fakeFeedRequestManager = scope.getFakeFeedRequestManager();
        modelProviderFactory = scope.getModelProviderFactory();
        protocolAdapter = scope.getProtocolAdapter();
        modelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
    }

    /**
     * Test steps:
     *
     * <ol>
     *   <li>Create the initial ModelProvider from $HEAD with a REQUEST_WITH_WAIT which makes the
     *       request before the session is populated.
     *   <li>Load the second request into the FeedRequestManager
     *   <li>Create a second ModelProvider using REQUEST_WITH_CONTENT which displays the initial
     * $HEAD but makes a request. The second request will be appended to and update the
     * ModelProvider.
     * </ol>
     */
    @Test
    public void testRequestWithWait() throws TimeoutException {
        // Load up the initial request
        fakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_ONE).build(), /* delayMs= */ 100);

        // Wait for the request to complete (REQUEST_WITH_CONTENT).  This will trigger the request
        // and wait for it to complete to populate the new session.
        fakeSchedulerApi.setRequestBehavior(RequestBehavior.REQUEST_WITH_WAIT);
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        // This will wait for the session to be created and validate the root cursor
        AtomicBoolean finished = new AtomicBoolean(false);
        assertSessionCreation(modelProvider, finished);
        long startTimeMs = fakeClock.currentTimeMillis();
        while (!finished.get()) {
            // Loop through the tasks and wait for the assertSessionCreation to set finished to true
            fakeClock.tick();
            if (timeoutDeadline > 0 && fakeClock.currentTimeMillis() > timeoutDeadline) {
                throw new TimeoutException();
            }
        }
        assertThat(fakeClock.currentTimeMillis() - startTimeMs).isAtLeast(100L);

        // Create a new ModelProvider from HEAD (REQUEST_WITH_CONTENT)
        fakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_TWO).build(), /* delayMs= */ 100);
        fakeSchedulerApi.setRequestBehavior(RequestBehavior.REQUEST_WITH_CONTENT);
        // This will wait for the session to be created and validate the root cursor
        modelProvider = modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertSessionCreationWithRequest(modelProvider, finished);
        startTimeMs = fakeClock.currentTimeMillis();
        while (!finished.get()) {
            // Loop through the tasks and wait for the assertSessionCreation to set finished to true
            fakeClock.tick();
            if (timeoutDeadline > 0 && fakeClock.currentTimeMillis() > timeoutDeadline) {
                throw new TimeoutException();
            }
        }
        assertThat(fakeClock.currentTimeMillis() - startTimeMs).isAtLeast(100L);
    }

    // Verifies the initial session.
    private void assertSessionCreation(ModelProvider modelProvider, AtomicBoolean finished) {
        finished.set(false);
        timeoutDeadline = DEBUG
                ? InfraIntegrationScope.TIMEOUT_TEST_TIMEOUT + fakeClock.currentTimeMillis()
                : 0;
        modelProvider.registerObserver(new ModelProviderObserver() {
            @Override
            public void onSessionStart(UiContext uiContext) {
                System.out.println("onSessionStart");
                finished.set(true);
                modelValidator.assertCursorContents(modelProvider, REQUEST_ONE);
            }

            @Override
            public void onSessionFinished(UiContext uiContext) {
                System.out.println("onSessionFinished");
            }

            @Override
            public void onError(ModelError modelError) {
                System.out.println("onError");
            }
        });
    }

    // Verifies the second session.  There are two observers verified, the ModelProvider READ
    // and a change listener on the root for the second request.
    private void assertSessionCreationWithRequest(
            ModelProvider modelProvider, AtomicBoolean finished) {
        finished.set(false);
        timeoutDeadline = DEBUG
                ? InfraIntegrationScope.TIMEOUT_TEST_TIMEOUT + fakeClock.currentTimeMillis()
                : 0;
        modelProvider.registerObserver(new ModelProviderObserver() {
            @Override
            public void onSessionStart(UiContext uiContext) {
                System.out.println("onSessionStart");
                ModelFeature feature = modelProvider.getRootFeature();
                // The second request will cause an change on the root
                assertThat(feature).isNotNull();
                feature.registerObserver(change -> {
                    System.out.println("root.onChange");
                    finished.set(true);
                    modelValidator.assertCursorContents(modelProvider, REQUEST_ONE[0],
                            REQUEST_ONE[1], REQUEST_ONE[2], REQUEST_TWO[0], REQUEST_TWO[2]);
                    assertThat(change.getContentId())
                            .isEqualTo(protocolAdapter.getStreamContentId(ROOT_CONTENT_ID));
                    ChildChanges childChanges = change.getChildChanges();
                    assertThat(childChanges.getAppendedChildren().size()).isEqualTo(2);
                });
                modelValidator.assertCursorContents(modelProvider, REQUEST_ONE);
            }

            @Override
            public void onSessionFinished(UiContext uiContext) {
                System.out.println("onSessionFinished");
            }

            @Override
            public void onError(ModelError modelError) {
                System.out.println("onError");
            }
        });
    }
}
