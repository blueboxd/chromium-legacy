// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.Arrays;

/** The Java equivalent to {@code ValueProto}. */
@JNINamespace("autofill_assistant")
public class AssistantValue {
    private final String[] mStrings;
    private final boolean[] mBooleans;
    private final int[] mIntegers;

    AssistantValue() {
        mStrings = null;
        mBooleans = null;
        mIntegers = null;
    }

    public AssistantValue(String[] strings) {
        mStrings = strings;
        mBooleans = null;
        mIntegers = null;
    }

    public AssistantValue(boolean[] booleans) {
        mStrings = null;
        mBooleans = booleans;
        mIntegers = null;
    }

    public AssistantValue(int[] integers) {
        mStrings = null;
        mBooleans = null;
        mIntegers = integers;
    }

    @CalledByNative
    public static AssistantValue create() {
        return new AssistantValue();
    }

    @CalledByNative
    public static AssistantValue createForStrings(String[] values) {
        return new AssistantValue(values);
    }

    @CalledByNative
    public static AssistantValue createForBooleans(boolean[] values) {
        return new AssistantValue(values);
    }

    @CalledByNative
    public static AssistantValue createForIntegers(int[] values) {
        return new AssistantValue(values);
    }

    @CalledByNative
    public String[] getStrings() {
        return mStrings;
    }

    @CalledByNative
    public boolean[] getBooleans() {
        return mBooleans;
    }

    @CalledByNative
    public int[] getIntegers() {
        return mIntegers;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj == this) {
            return true;
        }
        if (!(obj instanceof AssistantValue)) {
            return false;
        }
        AssistantValue value = (AssistantValue) obj;

        return Arrays.equals(this.mStrings, value.mStrings)
                && Arrays.equals(this.mBooleans, value.mBooleans)
                && Arrays.equals(this.mIntegers, value.mIntegers);
    }
}
