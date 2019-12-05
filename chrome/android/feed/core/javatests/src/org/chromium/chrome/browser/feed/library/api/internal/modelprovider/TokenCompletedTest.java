// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link TokenCompleted} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TokenCompletedTest {
    @Mock
    private ModelCursor mModelCursor;

    @Before
    public void setUp() {
        initMocks(this);
    }

    @Test
    public void testTokenChange() {
        TokenCompleted tokenCompleted = new TokenCompleted(mModelCursor);
        assertThat(tokenCompleted.getCursor()).isEqualTo(mModelCursor);
    }
}
