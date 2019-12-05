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

import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi.RequestBehavior;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.State;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.SessionTestUtils;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests the REQUEST_WITH_WAIT behavior for creating a new session. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class RequestWithWaitTest {
    private final SessionTestUtils mUtils = new SessionTestUtils(RequestBehavior.REQUEST_WITH_WAIT);
    private final InfraIntegrationScope mScope = mUtils.getScope();

    @Before
    public void setUp() {
        mUtils.populateHead();
    }

    @After
    public void tearDown() {
        mUtils.assertWorkComplete();
    }

    @Test
    public void test_hasContentWithRequest_spinnerThenShowContent() {
        long delayMs = mUtils.startOutstandingRequest();

        ModelProvider modelProvider = mUtils.createNewSession();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

        mScope.getFakeClock().advance(delayMs);
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        mUtils.assertNewContent(modelProvider.getAllRootChildren());
    }

    @Test
    public void test_hasContentWithRequest_spinnerThenShowContentOnFailure() {
        long delayMs = mUtils.startOutstandingRequestWithError();

        ModelProvider modelProvider = mUtils.createNewSession();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

        mScope.getFakeClock().advance(delayMs);
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        mUtils.assertHeadContent(modelProvider.getAllRootChildren());
    }

    @Test
    public void test_noContentWithRequest_spinnerThenShowContent() {
        mScope.getAppLifecycleListener().onClearAll();
        long delayMs = mUtils.startOutstandingRequest();

        ModelProvider modelProvider = mUtils.createNewSession();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

        mScope.getFakeClock().advance(delayMs);
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        mUtils.assertNewContent(modelProvider.getAllRootChildren());
    }

    @Test
    public void test_noContentWithRequest_spinnerThenZeroStateOnFailure() {
        mScope.getAppLifecycleListener().onClearAll();
        long delayMs = mUtils.startOutstandingRequestWithError();

        ModelProvider modelProvider = mUtils.createNewSession();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

        mScope.getFakeClock().advance(delayMs);
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        assertThat(modelProvider.getAllRootChildren()).isEmpty();
    }

    @Test
    public void test_noContentNoRequest_spinnerThenShowContent() {
        mScope.getAppLifecycleListener().onClearAll();
        long delayMs = mUtils.queueRequest();

        ModelProvider modelProvider = mUtils.createNewSession();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

        mScope.getFakeClock().advance(delayMs);
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        mUtils.assertNewContent(modelProvider.getAllRootChildren());
    }

    @Test
    public void test_noContentNoRequest_spinnerThenZeroStateOnFailure() {
        mScope.getAppLifecycleListener().onClearAll();
        long delayMs = mUtils.queueError();

        ModelProvider modelProvider = mUtils.createNewSession();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

        mScope.getFakeClock().advance(delayMs);
        assertThat(modelProvider.getAllRootChildren()).isEmpty();
    }

    @Test
    public void test_hasContentNoRequest_spinnerThenShowContent() {
        long delayMs = mUtils.queueRequest();

        ModelProvider modelProvider = mUtils.createNewSession();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

        mScope.getFakeClock().advance(delayMs);
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        mUtils.assertNewContent(modelProvider.getAllRootChildren());
    }

    @Test
    public void test_hasContentNoRequest_spinnerThenShowContentOnFailure() {
        long delayMs = mUtils.queueError();

        ModelProvider modelProvider = mUtils.createNewSession();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

        mScope.getFakeClock().advance(delayMs);
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        mUtils.assertHeadContent(modelProvider.getAllRootChildren());
    }
}
