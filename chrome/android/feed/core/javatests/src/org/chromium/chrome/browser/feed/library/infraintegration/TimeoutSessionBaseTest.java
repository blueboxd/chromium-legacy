// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi.RequestBehavior;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderObserver;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.FeedSessionManagerImpl;
import org.chromium.chrome.browser.feed.library.testing.host.scheduler.FakeSchedulerApi;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This is a TimeoutSession test which verifies the REQUEST_WITH_WAIT.
 *
 * <p>NOTE: This test has multiple threads running. There is a single threaded executor created in
 * addition to the main thread. The Test will throw a TimeoutException in in production in the event
 * of a deadlock. The DEBUG boolean controls the behavior to allow debugging.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TimeoutSessionBaseTest {
    // This flag will should be flipped to debug the test.  It will disable TimeoutExceptions.
    private static final boolean DEBUG = false;

    private final FakeSchedulerApi mFakeSchedulerApi =
            new FakeSchedulerApi(FakeThreadUtils.withoutThreadChecks());

    private FakeClock mFakeClock;
    private FakeFeedRequestManager mFakeFeedRequestManager;
    private FeedSessionManager mFeedSessionManager;
    private ModelProviderFactory mModelProviderFactory;
    private ModelProviderValidator mModelValidator;
    private long mTimeoutDeadline;

    @Before
    public void setUp() {
        initMocks(this);
        InfraIntegrationScope scope = new InfraIntegrationScope.Builder()
                                              .setSchedulerApi(mFakeSchedulerApi)
                                              .withTimeoutSessionConfiguration(2L)
                                              .build();
        mFakeClock = scope.getFakeClock();
        mFakeFeedRequestManager = scope.getFakeFeedRequestManager();
        mFeedSessionManager = scope.getFeedSessionManager();
        mModelProviderFactory = scope.getModelProviderFactory();
        mModelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
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
        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(requestOne).build(), /* delayMs= */ 100);

        // Wait for the request to complete (REQUEST_WITH_WAIT).  This will trigger the request and
        // wait for it to complete to populate the new session.
        mFakeSchedulerApi.setRequestBehavior(RequestBehavior.REQUEST_WITH_WAIT);
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        // This will wait for the session to be created and validate the root cursor
        AtomicBoolean finished = new AtomicBoolean(false);
        assertSessionCreation(modelProvider, finished, requestOne);
        long startTimeMs = mFakeClock.currentTimeMillis();
        while (!finished.get()) {
            // Loop through the tasks and wait for the assertSessionCreation to set finished to true
            mFakeClock.tick();
            if (mTimeoutDeadline > 0 && mFakeClock.currentTimeMillis() > mTimeoutDeadline) {
                throw new TimeoutException();
            }
        }
        assertThat(mFakeClock.currentTimeMillis() - startTimeMs).isAtLeast(100L);

        // Create a new ModelProvider from HEAD (NO_REQUEST_WITH_CONTENT)
        mFakeSchedulerApi.setRequestBehavior(RequestBehavior.NO_REQUEST_WITH_CONTENT);
        // This will wait for the session to be created and validate the root cursor
        modelProvider = mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertSessionCreation(modelProvider, finished, requestOne);
        startTimeMs = mFakeClock.currentTimeMillis();
        while (!finished.get()) {
            // Loop through the tasks and wait for the assertSessionCreation to set finished to true
            mFakeClock.tick();
            if (mTimeoutDeadline > 0 && mFakeClock.currentTimeMillis() > mTimeoutDeadline) {
                throw new TimeoutException();
            }
        }
        assertThat(mFakeClock.currentTimeMillis() - startTimeMs).isEqualTo(0);
    }

    private void assertSessionCreation(
            ModelProvider modelProvider, AtomicBoolean finished, ContentId... cards) {
        finished.set(false);
        mTimeoutDeadline = DEBUG
                ? InfraIntegrationScope.TIMEOUT_TEST_TIMEOUT + mFakeClock.currentTimeMillis()
                : 0;
        modelProvider.registerObserver(new ModelProviderObserver() {
            @Override
            public void onSessionStart(UiContext uiContext) {
                System.out.println("onSessionStart");
                finished.set(true);
                mModelValidator.assertCursorContents(modelProvider, cards);
                assertThat(((FeedSessionManagerImpl) mFeedSessionManager).isDelayed()).isFalse();
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
