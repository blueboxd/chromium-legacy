// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.storage;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult.Result;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Test class for {@link CommitResult} */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CommitResultTest {
    @Test
    public void testSuccess() {
        assertThat(CommitResult.SUCCESS.getResult()).isEqualTo(Result.SUCCESS);
    }

    @Test
    public void testSuccessSingleton() {
        CommitResult success = CommitResult.SUCCESS;
        assertThat(success).isEqualTo(CommitResult.SUCCESS);
    }

    @Test
    public void testFailure() {
        assertThat(CommitResult.FAILURE.getResult()).isEqualTo(Result.FAILURE);
    }

    @Test
    public void testFailureSingleton() {
        CommitResult failure = CommitResult.FAILURE;
        assertThat(failure).isEqualTo(CommitResult.FAILURE);
    }
}
