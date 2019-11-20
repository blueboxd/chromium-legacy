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

/** Tests the REQUEST_WITH_WAIT behavior for creating a new session. */
@RunWith(RobolectricTestRunner.class)
public final class RequestWithWaitTest {
  private final SessionTestUtils utils = new SessionTestUtils(RequestBehavior.REQUEST_WITH_WAIT);
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
    long delayMs = utils.startOutstandingRequest();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(delayMs);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    utils.assertNewContent(modelProvider.getAllRootChildren());
  }

  @Test
  public void test_hasContentWithRequest_spinnerThenShowContentOnFailure() {
    long delayMs = utils.startOutstandingRequestWithError();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(delayMs);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    utils.assertHeadContent(modelProvider.getAllRootChildren());
  }

  @Test
  public void test_noContentWithRequest_spinnerThenShowContent() {
    scope.getAppLifecycleListener().onClearAll();
    long delayMs = utils.startOutstandingRequest();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(delayMs);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    utils.assertNewContent(modelProvider.getAllRootChildren());
  }

  @Test
  public void test_noContentWithRequest_spinnerThenZeroStateOnFailure() {
    scope.getAppLifecycleListener().onClearAll();
    long delayMs = utils.startOutstandingRequestWithError();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(delayMs);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    assertThat(modelProvider.getAllRootChildren()).isEmpty();
  }

  @Test
  public void test_noContentNoRequest_spinnerThenShowContent() {
    scope.getAppLifecycleListener().onClearAll();
    long delayMs = utils.queueRequest();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(delayMs);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    utils.assertNewContent(modelProvider.getAllRootChildren());
  }

  @Test
  public void test_noContentNoRequest_spinnerThenZeroStateOnFailure() {
    scope.getAppLifecycleListener().onClearAll();
    long delayMs = utils.queueError();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(delayMs);
    assertThat(modelProvider.getAllRootChildren()).isEmpty();
  }

  @Test
  public void test_hasContentNoRequest_spinnerThenShowContent() {
    long delayMs = utils.queueRequest();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(delayMs);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    utils.assertNewContent(modelProvider.getAllRootChildren());
  }

  @Test
  public void test_hasContentNoRequest_spinnerThenShowContentOnFailure() {
    long delayMs = utils.queueError();

    ModelProvider modelProvider = utils.createNewSession();
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.INITIALIZING);

    scope.getFakeClock().advance(delayMs);
    assertThat(modelProvider.getCurrentState()).isEqualTo(State.READY);
    utils.assertHeadContent(modelProvider.getAllRootChildren());
  }
}
