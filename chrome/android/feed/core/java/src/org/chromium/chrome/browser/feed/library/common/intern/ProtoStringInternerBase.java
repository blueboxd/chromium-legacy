// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.intern;

import android.annotation.SuppressLint;

import androidx.annotation.Nullable;

import com.google.protobuf.GeneratedMessageLite;
import com.google.protobuf.MessageLite;

import java.util.ArrayList;
import java.util.List;

import javax.annotation.concurrent.ThreadSafe;

/**
 * A string-specific {@link Interner} base implementation that provides common helper methods to
 * help in proto string interning.
 */
@ThreadSafe
public abstract class ProtoStringInternerBase<P extends MessageLite> implements Interner<P> {
    private final Interner<String> mInterner;

    protected ProtoStringInternerBase(Interner<String> interner) {
        this.mInterner = interner;
    }

    protected interface SingleStringFieldGetter<T extends GeneratedMessageLite<T, ?>> {
        String getField(T input);
    }

    protected interface SingleStringFieldSetter<B extends GeneratedMessageLite.Builder<?, B>> {
        void setField(B builder, String value);
    }

    @SuppressWarnings("ReferenceEquality") // Intentional reference comparison for interned != orig
    @SuppressLint("StringEquality")
    @Nullable
    protected <T extends GeneratedMessageLite<T, B>, B extends GeneratedMessageLite.Builder<T, B>> B
    internSingleStringField(T input, @Nullable B builder,
            SingleStringFieldGetter<T> singleStringFieldGetter,
            SingleStringFieldSetter<B> singleStringFieldSetter) {
        String orig = singleStringFieldGetter.getField(input);
        String interned = mInterner.intern(orig);
        if (interned != orig) {
            builder = ensureBuilder(input, builder);
            singleStringFieldSetter.setField(builder, interned);
        }
        return builder;
    }

    protected interface RepeatedStringFieldGetter<T extends GeneratedMessageLite<T, ?>> {
        List<String> getField(T input);
    }

    protected interface RepeatedStringFieldClearer<B extends GeneratedMessageLite.Builder<?, B>> {
        void clearField(B builder);
    }

    protected interface RepeatedStringFieldAllAdder<B extends GeneratedMessageLite.Builder<?, B>> {
        void addAllField(B builder, List<String> value);
    }

    @SuppressWarnings("ReferenceEquality") // Intentional reference comparison for interned != orig
    @SuppressLint("StringEquality")
    @Nullable
    protected <T extends GeneratedMessageLite<T, B>, B extends GeneratedMessageLite.Builder<T, B>> B
    internRepeatedStringField(T input, @Nullable B builder,
            RepeatedStringFieldGetter<T> repeatedStringFieldGetter,
            RepeatedStringFieldClearer<B> repeatedStringFieldClearer,
            RepeatedStringFieldAllAdder<B> repeatedStringFieldAllAdder) {
        boolean modified = false;
        List<String> internedValues = new ArrayList<>();
        for (String orig : repeatedStringFieldGetter.getField(input)) {
            String interned = mInterner.intern(orig);
            internedValues.add(interned);
            if (interned != orig) {
                modified = true;
            }
        }
        if (modified) {
            builder = ensureBuilder(input, builder);
            repeatedStringFieldClearer.clearField(builder);
            repeatedStringFieldAllAdder.addAllField(builder, internedValues);
        }
        return builder;
    }

    protected <T extends GeneratedMessageLite<T, B>, B extends GeneratedMessageLite.Builder<T, B>> B
    ensureBuilder(T input, @Nullable B builder) {
        if (builder == null) {
            builder = input.toBuilder();
        }
        return builder;
    }

    @Override
    public void clear() {
        mInterner.clear();
    }

    @Override
    public int size() {
        return mInterner.size();
    }
}
