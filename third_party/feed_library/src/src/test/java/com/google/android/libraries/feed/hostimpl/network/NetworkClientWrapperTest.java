// Copyright 2018 The Feed Authors.
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
package com.google.android.libraries.feed.hostimpl.network;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.validateMockitoUsage;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.net.Uri;
import com.google.android.libraries.feed.api.host.network.HttpRequest;
import com.google.android.libraries.feed.api.host.network.HttpRequest.HttpMethod;
import com.google.android.libraries.feed.api.host.network.HttpResponse;
import com.google.android.libraries.feed.api.host.network.NetworkClient;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.functional.Consumer;
import java.util.Collections;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link NetworkClientWrapper}. */
@RunWith(RobolectricTestRunner.class)
public class NetworkClientWrapperTest {
  @Mock private ThreadUtils threadUtils;
  @Mock private NetworkClient networkClient;
  @Mock private Consumer<HttpResponse> responseConsumer;

  private HttpRequest request;
  private final FakeMainThreadRunner mainThreadRunner = FakeMainThreadRunner.queueAllTasks();

  @Before
  public void setup() {
    request = new HttpRequest(Uri.EMPTY, HttpMethod.GET, Collections.emptyList(), new byte[] {});
    initMocks(this);
  }

  @After
  public void validate() {
    validateMockitoUsage();
  }

  @Test
  public void testSend_mainThread() {
    when(threadUtils.isMainThread()).thenReturn(true);
    NetworkClientWrapper wrapper =
        new NetworkClientWrapper(networkClient, threadUtils, mainThreadRunner);
    wrapper.send(request, responseConsumer);
    verify(networkClient).send(request, responseConsumer);
    assertThat(mainThreadRunner.hasTasks()).isFalse();
  }

  @Test
  public void testSend_backgroundThread() {
    when(threadUtils.isMainThread()).thenReturn(false);
    NetworkClientWrapper wrapper =
        new NetworkClientWrapper(networkClient, threadUtils, mainThreadRunner);
    wrapper.send(request, responseConsumer);

    assertThat(mainThreadRunner.hasTasks()).isTrue();
    mainThreadRunner.runAllTasks();

    verify(networkClient).send(request, responseConsumer);
  }
}
