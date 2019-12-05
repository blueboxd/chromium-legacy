// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder.ROOT_CONTENT_ID;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi.RequestBehavior;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChange.ChildChanges;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderObserver;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ModelProviderValidator;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.testing.host.scheduler.FakeSchedulerApi;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This is a TimeoutSession test which verifies REQUEST_WITH_CONTENT
 *
 * <p>NOTE: This test has multiple threads running. There is a single threaded executor created in
 * addition to the main thread. The Test will throw a TimeoutException in in production in the event
 * of a deadlock. The DEBUG boolean controls the behavior to allow debugging.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TimeoutSessionWithContentTest {
    // This flag will should be flipped to debug the test.  It will disable TimeoutExceptions.
    private static final boolean DEBUG = false;
    private static final ContentId[] REQUEST_ONE = new ContentId[] {
            ResponseBuilder.createFeatureContentId(1), ResponseBuilder.createFeatureContentId(2),
            ResponseBuilder.createFeatureContentId(3)};
    private static final ContentId[] REQUEST_TWO = new ContentId[] {
            ResponseBuilder.createFeatureContentId(4), ResponseBuilder.createFeatureContentId(3),
            ResponseBuilder.createFeatureContentId(5)};

    private final FakeSchedulerApi mFakeSchedulerApi =
            new FakeSchedulerApi(FakeThreadUtils.withoutThreadChecks());

    private FakeClock mFakeClock;
    private FakeFeedRequestManager mFakeFeedRequestManager;
    private ModelProviderFactory mModelProviderFactory;
    private ProtocolAdapter mProtocolAdapter;
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
        mModelProviderFactory = scope.getModelProviderFactory();
        mProtocolAdapter = scope.getProtocolAdapter();
        mModelValidator = new ModelProviderValidator(scope.getProtocolAdapter());
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
        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_ONE).build(), /* delayMs= */ 100);

        // Wait for the request to complete (REQUEST_WITH_CONTENT).  This will trigger the request
        // and wait for it to complete to populate the new session.
        mFakeSchedulerApi.setRequestBehavior(RequestBehavior.REQUEST_WITH_WAIT);
        ModelProvider modelProvider =
                mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());

        // This will wait for the session to be created and validate the root cursor
        AtomicBoolean finished = new AtomicBoolean(false);
        assertSessionCreation(modelProvider, finished);
        long startTimeMs = mFakeClock.currentTimeMillis();
        while (!finished.get()) {
            // Loop through the tasks and wait for the assertSessionCreation to set finished to true
            mFakeClock.tick();
            if (mTimeoutDeadline > 0 && mFakeClock.currentTimeMillis() > mTimeoutDeadline) {
                throw new TimeoutException();
            }
        }
        assertThat(mFakeClock.currentTimeMillis() - startTimeMs).isAtLeast(100L);

        // Create a new ModelProvider from HEAD (REQUEST_WITH_CONTENT)
        mFakeFeedRequestManager.queueResponse(
                ResponseBuilder.forClearAllWithCards(REQUEST_TWO).build(), /* delayMs= */ 100);
        mFakeSchedulerApi.setRequestBehavior(RequestBehavior.REQUEST_WITH_CONTENT);
        // This will wait for the session to be created and validate the root cursor
        modelProvider = mModelProviderFactory.createNew(null, UiContext.getDefaultInstance());
        assertSessionCreationWithRequest(modelProvider, finished);
        startTimeMs = mFakeClock.currentTimeMillis();
        while (!finished.get()) {
            // Loop through the tasks and wait for the assertSessionCreation to set finished to true
            mFakeClock.tick();
            if (mTimeoutDeadline > 0 && mFakeClock.currentTimeMillis() > mTimeoutDeadline) {
                throw new TimeoutException();
            }
        }
        assertThat(mFakeClock.currentTimeMillis() - startTimeMs).isAtLeast(100L);
    }

    // Verifies the initial session.
    private void assertSessionCreation(ModelProvider modelProvider, AtomicBoolean finished) {
        finished.set(false);
        mTimeoutDeadline = DEBUG
                ? InfraIntegrationScope.TIMEOUT_TEST_TIMEOUT + mFakeClock.currentTimeMillis()
                : 0;
        modelProvider.registerObserver(new ModelProviderObserver() {
            @Override
            public void onSessionStart(UiContext uiContext) {
                System.out.println("onSessionStart");
                finished.set(true);
                mModelValidator.assertCursorContents(modelProvider, REQUEST_ONE);
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
        mTimeoutDeadline = DEBUG
                ? InfraIntegrationScope.TIMEOUT_TEST_TIMEOUT + mFakeClock.currentTimeMillis()
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
                    mModelValidator.assertCursorContents(modelProvider, REQUEST_ONE[0],
                            REQUEST_ONE[1], REQUEST_ONE[2], REQUEST_TWO[0], REQUEST_TWO[2]);
                    assertThat(change.getContentId())
                            .isEqualTo(mProtocolAdapter.getStreamContentId(ROOT_CONTENT_ID));
                    ChildChanges childChanges = change.getChildChanges();
                    assertThat(childChanges.getAppendedChildren().size()).isEqualTo(2);
                });
                mModelValidator.assertCursorContents(modelProvider, REQUEST_ONE);
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
