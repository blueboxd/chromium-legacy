// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedrequestmanager;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.api.internal.requestmanager.FeedRequestManager;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Test of the {@link RequestManagerImpl} class. */
@RunWith(RobolectricTestRunner.class)
public final class RequestManagerImplTest {
    @Mock
    private FeedRequestManager feedRequestManager;
    @Mock
    private FeedSessionManager feedSessionManager;
    @Mock
    private Consumer<Result<Model>> updateConsumer;

    private RequestManagerImpl requestManager;

    @Before
    public void createRequestManager() {
        initMocks(this);
        requestManager = new RequestManagerImpl(feedRequestManager, feedSessionManager);
        when(feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT))
                .thenReturn(updateConsumer);
    }

    @Test
    public void testTriggerScheduledRefresh() {
        requestManager.triggerScheduledRefresh();

        verify(feedRequestManager).triggerRefresh(RequestReason.HOST_REQUESTED, updateConsumer);
    }
}
