// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.modelprovider;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link TokenCompleted} class. */
@RunWith(RobolectricTestRunner.class)
public class TokenCompletedTest {
    @Mock
    private ModelCursor modelCursor;

    @Before
    public void setUp() {
        initMocks(this);
    }

    @Test
    public void testTokenChange() {
        TokenCompleted tokenCompleted = new TokenCompleted(modelCursor);
        assertThat(tokenCompleted.getCursor()).isEqualTo(modelCursor);
    }
}
