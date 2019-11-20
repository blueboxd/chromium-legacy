// Copyright 2019 The Feed Authors.
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

/** Tests the NO_REQUEST_WITH_TIMEOUT behavior for creating a new session. */
@RunWith(RobolectricTestRunner.class)
public final class NoRequestWithTimeoutTest {
  private final SessionTestUtils utils =
      new SessionTestUtils(RequestBehavior.NO_REQUEST_WITH_TIMEOUT);
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
  public void test_hasContentWithRequest_spinnerThenShowContent() {
    long delayMs = utils.requestBeforeTimeout().startOutstandingRequest();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(delayMs);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    utils.assertNewContent(modelProvider.getAllRootChildren());
  }

  @Test
  public void test_hasContentWithRequest_spinnerThenShowContentWithError() {
    long delayMs = utils.requestBeforeTimeout().startOutstandingRequestWithError();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(delayMs);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    utils.assertHeadContent(modelProvider.getAllRootChildren());
  }

  @Test
  public void test_hasContentWithRequest_spinnerThenShowContentOnTimeoutAndAppend() {
    long delayMs = utils.startOutstandingRequest();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(SessionTestUtils.TIMEOUT_MS);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    utils.assertHeadContent(modelProvider.getAllRootChildren());

    scope.getFakeClock().advanceTo(delayMs);
    utils.assertAppendedContent(modelProvider.getAllRootChildren());
  }

  @Test
  public void test_hasContentWithRequest_spinnerThenShowContentOnTimeoutWithError() {
    long delayMs = utils.startOutstandingRequestWithError();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(SessionTestUtils.TIMEOUT_MS);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    utils.assertHeadContent(modelProvider.getAllRootChildren());

    scope.getFakeClock().advanceTo(delayMs);
    utils.assertHeadContent(modelProvider.getAllRootChildren());
  }

  @Test
  public void test_noContentWithRequest_spinnerThenShowContent() {
    scope.getAppLifecycleListener().onClearAll();
    long delayMs = utils.requestBeforeTimeout().startOutstandingRequest();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(delayMs);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    utils.assertNewContent(modelProvider.getAllRootChildren());
  }

  @Test
  public void test_noContentWithRequest_spinnerThenZeroStateWithError() {
    scope.getAppLifecycleListener().onClearAll();
    long delayMs = utils.requestBeforeTimeout().startOutstandingRequestWithError();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(delayMs);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    assertThat(modelProvider.getAllRootChildren()).isEmpty();
  }

  @Test
  public void test_noContentWithRequest_spinnerThenZeroStateOnTimeout() {
    scope.getAppLifecycleListener().onClearAll();
    long delayMs = utils.startOutstandingRequest();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(SessionTestUtils.TIMEOUT_MS);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    assertThat(modelProvider.getAllRootChildren()).isEmpty();

    scope.getFakeClock().advanceTo(delayMs);
    assertThat(modelProvider.getAllRootChildren()).isEmpty();
  }

  @Test
  public void test_noContentWithRequest_spinnerThenZeroStateOnTimeoutWithError() {
    scope.getAppLifecycleListener().onClearAll();
    long delayMs = utils.startOutstandingRequestWithError();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(SessionTestUtils.TIMEOUT_MS);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    assertThat(modelProvider.getAllRootChildren()).isEmpty();

    scope.getFakeClock().advanceTo(delayMs);
    assertThat(modelProvider.getAllRootChildren()).isEmpty();
  }

  @Test
  public void test_noContentNoRequest_shouldShowZeroState() {
    scope.getAppLifecycleListener().onClearAll();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    assertThat(modelProvider.getAllRootChildren()).isEmpty();
  }

  @Test
  public void test_hasContentNoRequest_shouldShowContentImmediately() {
    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    utils.assertHeadContent(modelProvider.getAllRootChildren());
  }
}
