// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedrequestmanager;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.requestmanager.FeedRequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Test of the {@link RequestManagerImpl} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class RequestManagerImplTest {
    @Mock
    private FeedRequestManager mFeedRequestManager;
    @Mock
    private FeedSessionManager mFeedSessionManager;
    @Mock
    private Consumer<Result<Model>> mUpdateConsumer;

    private RequestManagerImpl mRequestManager;

    @Before
    public void createRequestManager() {
        initMocks(this);
        mRequestManager = new RequestManagerImpl(mFeedRequestManager, mFeedSessionManager);
        when(mFeedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT))
                .thenReturn(mUpdateConsumer);
    }

    @Test
    public void testTriggerScheduledRefresh() {
        mRequestManager.triggerScheduledRefresh();

        verify(mFeedRequestManager).triggerRefresh(RequestReason.HOST_REQUESTED, mUpdateConsumer);
    }
}
