// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi.RequestBehavior;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderObserver;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.ModelProviderValidator;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.feedsessionmanager.FeedSessionManagerImpl;
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
 * This is a TimeoutSession test which verifies the REQUEST_WITH_WAIT.
 *
 * <p>NOTE: This test has multiple threads running. There is a single threaded executor created in
 * addition to the main thread. The Test will throw a TimeoutException in in production in the event
 * of a deadlock. The DEBUG boolean controls the behavior to allow debugging.
 */
@RunWith(RobolectricTestRunner.class)
public class TimeoutSessionBaseTest {
    // This flag will should be flipped to debug the test.  It will disable TimeoutExceptions.
    private static final boolean DEBUG;

    private final FakeSchedulerApi fakeSchedulerApi =
            new FakeSchedulerApi(FakeThreadUtils.withoutThreadChecks());

    private FakeClock fakeClock;
    private FakeFeedRequestManager fakeFeedRequestManager;
    private FeedSessionManager feedSessionManager;
    private ModelProviderFactory modelProviderFactory;
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
        feedSessionManager = scope.getFeedSessionManager();
        modelProviderFactory = scope.getModelProviderFactory();
        modelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
    }

    /**
     * Test steps:
     *
     * <ol>
     *   <li>Create the initial ModelProvider from $HEAD with a REQUEST_WITH_WAIT which makes the
     *       request before the session is populated.
     *   <li>Create a second ModelProvider using NO_REQUEST_WITH_CONTENT which should duplidate the
     *       session created with the initial request.
     * </ol>
     */
    @Test
    public void testRequestWithWait() throws TimeoutException {
        ContentId[] requestOne = new ContentId[] {ResponseBuilder.createFeatureContentId(1),
                ResponseBuilder.createFeatureContentId(2),
                ResponseBuilder.createFeatureContentId(3)};

        // Load up the initial request
        fakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(requestOne).build(), /* delayMs= */ 100);

        // Wait for the request to complete (REQUEST_WITH_WAIT).  This will trigger the request and
        // wait for it to complete to populate the new session.
        fakeSchedulerApi.setRequestBehavior(RequestBehavior.REQUEST_WITH_WAIT);
        ModelProvider modelProvider =
                modelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        // This will wait for the session to be created and validate the root cursor
        AtomicBoolean finished = new AtomicBoolean(false);
        assertSessionCreation(modelProvider, finished, requestOne);
        long startTimeMs = fakeClock.currentTimeMillis();
        while (!finished.get()) {
            // Loop through the tasks and wait for the assertSessionCreation to set finished to true
            fakeClock.tick();
            if (timeoutDeadline > 0 && fakeClock.currentTimeMillis() > timeoutDeadline) {
                throw new TimeoutException();
            }
        }
        assertThat(fakeClock.currentTimeMillis() - startTimeMs).isAtLeast(100L);

        // Create a new ModelProvider from HEAD (NO_REQUEST_WITH_CONTENT)
        fakeSchedulerApi.setRequestBehavior(RequestBehavior.NO_REQUEST_WITH_CONTENT);
        // This will wait for the session to be created and validate the root cursor
        modelProvider = modelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertSessionCreation(modelProvider, finished, requestOne);
        startTimeMs = fakeClock.currentTimeMillis();
        while (!finished.get()) {
            // Loop through the tasks and wait for the assertSessionCreation to set finished to true
            fakeClock.tick();
            if (timeoutDeadline > 0 && fakeClock.currentTimeMillis() > timeoutDeadline) {
                throw new TimeoutException();
            }
        }
        assertThat(fakeClock.currentTimeMillis() - startTimeMs).isEqualTo(0);
    }

    private void assertSessionCreation(
            ModelProvider modelProvider, AtomicBoolean finished, ContentId... cards) {
        finished.set(false);
        timeoutDeadline = DEBUG
                ? InfraIntegrationScope.TIMEOUT_TEST_TIMEOUT + fakeClock.currentTimeMillis()
                : 0;
        modelProvider.registerObserver(new ModelProviderObserver() {
            @Override
            public void onSessionStart(UiContext uiContext) {
                System.out.println("onSessionStart");
                finished.set(true);
                modelValidator.assertCursorContents(modelProvider, cards);
                assertThat(((FeedSessionManagerImpl) feedSessionManager).isDelayed()).isFalse();
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
