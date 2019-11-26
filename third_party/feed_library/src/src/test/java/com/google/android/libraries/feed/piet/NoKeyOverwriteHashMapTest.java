// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;

import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Tests of the NoKeyOverwriteHashMap */
@RunWith(JUnit4.class)
public class NoKeyOverwriteHashMapTest {
    private final NoKeyOverwriteHashMap<String, String> mMap =
            new NoKeyOverwriteHashMap<>("Acronym", ErrorCode.ERR_DUPLICATE_BINDING_VALUE);

    @Test
    public void testPutTwoDifferentKeys() {
        mMap.put("CPA", "Certified Public Accountant");
        mMap.put("CPU", "Central Processing Unit");
    }

    @Test
    public void testPutTwoSameKeysThrows() {
        mMap.put("CD", "Compact Disc");
        assertThatRunnable(() -> mMap.put("CD", "Certificate of Deposit"))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Acronym key 'CD' already defined");
    }
}
