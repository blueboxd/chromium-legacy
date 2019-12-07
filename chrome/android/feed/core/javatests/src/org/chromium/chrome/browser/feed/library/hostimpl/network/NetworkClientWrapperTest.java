// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.hostimpl.network;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.validateMockitoUsage;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.net.Uri;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest.HttpMethod;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpResponse;
import org.chromium.chrome.browser.feed.library.api.host.network.NetworkClient;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Collections;

/** Tests of the {@link NetworkClientWrapper}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NetworkClientWrapperTest {
    @Mock
    private ThreadUtils mThreadUtils;
    @Mock
    private NetworkClient mNetworkClient;
    @Mock
    private Consumer<HttpResponse> mResponseConsumer;

    private HttpRequest mRequest;
    private final FakeMainThreadRunner mMainThreadRunner = FakeMainThreadRunner.queueAllTasks();

    @Before
    public void setup() {
        mRequest =
                new HttpRequest(Uri.EMPTY, HttpMethod.GET, Collections.emptyList(), new byte[] {});
        initMocks(this);
    }

    @After
    public void validate() {
        validateMockitoUsage();
    }

    @Test
    public void testSend_mainThread() {
        when(mThreadUtils.isMainThread()).thenReturn(true);
        NetworkClientWrapper wrapper =
                new NetworkClientWrapper(mNetworkClient, mThreadUtils, mMainThreadRunner);
        wrapper.send(mRequest, mResponseConsumer);
        verify(mNetworkClient).send(mRequest, mResponseConsumer);
        assertThat(mMainThreadRunner.hasTasks()).isFalse();
    }

    @Test
    public void testSend_backgroundThread() {
        when(mThreadUtils.isMainThread()).thenReturn(false);
        NetworkClientWrapper wrapper =
                new NetworkClientWrapper(mNetworkClient, mThreadUtils, mMainThreadRunner);
        wrapper.send(mRequest, mResponseConsumer);

        assertThat(mMainThreadRunner.hasTasks()).isTrue();
        mMainThreadRunner.runAllTasks();

        verify(mNetworkClient).send(mRequest, mResponseConsumer);
    }
}
