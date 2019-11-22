// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi.RequestBehavior;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.State;
import com.google.android.libraries.feed.common.testing.InfraIntegrationScope;
import com.google.android.libraries.feed.common.testing.SessionTestUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests the REQUEST_WITH_CONTENT behavior for creating a new session. */
@RunWith(RobolectricTestRunner.class)
public final class RequestWithContentTest {
    private final SessionTestUtils utils =
            new SessionTestUtils(RequestBehavior.REQUEST_WITH_CONTENT);
    private final InfraIntegrationScope scope = utils.getScope();

    @Before
    public void setUp() {
        utils.populateHead();
    }

    @After
    public void tearDown() {
        utils.assertWorkComplete();
    }

    @Test
    public void test_hasContentWithRequest_shouldShowContentImmediatelyAndAppend() {
        long delayMs = utils.startOutstandingRequest();

        ModelProvider modelProvider = utils.createNewSession();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        utils.assertHeadContent(modelProvider.getAllRootChildren());

        // REQUEST_2 items should be appended.
        scope.getFakeClock().advance(delayMs);
        utils.assertAppendedContent(modelProvider.getAllRootChildren());
    }

    @Test
    public void test_noContentWithRequest_shouldShowZeroState() {
        scope.getAppLifecycleListener().onClearAll();
        long delayMs = utils.startOutstandingRequest();

        ModelProvider modelProvider = utils.createNewSession();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        assertThat(modelProvider.getAllRootChildren()).isEmpty();

        // REQUEST_2 items should not be appended.
        scope.getFakeClock().advance(delayMs);
        assertThat(modelProvider.getAllRootChildren()).isEmpty();
    }

    @Test
    public void test_noContentNoRequest_shouldShowZeroState() {
        scope.getAppLifecycleListener().onClearAll();
        long delayMs = utils.queueRequest();

        ModelProvider modelProvider = utils.createNewSession();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        assertThat(modelProvider.getAllRootChildren()).isEmpty();

        // REQUEST_2 items should not be appended.
        scope.getFakeClock().advance(delayMs);
        assertThat(modelProvider.getAllRootChildren()).isEmpty();
    }

    @Test
    public void test_hasContentNoRequest_shouldShowContentImmediatelyAndAppend() {
        long delayMs = utils.queueRequest();

        ModelProvider modelProvider = utils.createNewSession();
        assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
        utils.assertHeadContent(modelProvider.getAllRootChildren());

        // REQUEST_2 items should be appended.
        scope.getFakeClock().advance(delayMs);
        utils.assertAppendedContent(modelProvider.getAllRootChildren());
    }
}
