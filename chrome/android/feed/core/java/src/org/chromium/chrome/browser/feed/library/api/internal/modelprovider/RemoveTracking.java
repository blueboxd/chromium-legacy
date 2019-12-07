// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.common.functional.Function;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;

import java.util.ArrayList;
import java.util.List;

/**
 * Class which implements the filtering portion of remove tracking. This class creates a {@code
 * List<T>} of items using the {@code filterPredicate} function to both filter and transform a
 * {@link StreamFeature}. The function may return null, these values will be filtered.
 *
 * <p>The {@link #triggerConsumerUpdate()} will call the {@link Consumer}. It will be called when
 * all remove within the ModelProvider mutation have been processed.
 *
 * <p>The {@code Consumer} will be called on the main thread and called once per mutation. In
 * addition, the {@code Consumer} is called before the change operation.
 */
public final class RemoveTracking<T> {
    private final Function<StreamFeature, /*@Nullable*/ T> mFilterPredicate;
    private final Consumer<List<T>> mConsumer;
    private final List<T> mMatchingItems = new ArrayList<>();

    /**
     * Create the state necessary to call transform and filter the removed subtree before calling
     * the
     * {@link Consumer}.
     */
    public RemoveTracking(
            Function<StreamFeature, /*@Nullable*/ T> filterPredicate, Consumer<List<T>> consumer) {
        this.mFilterPredicate = filterPredicate;
        this.mConsumer = consumer;
    }

    /**
     * Called to transform and filter a {@link StreamFeature} found within a subtree being removed.
     */
    public void filterStreamFeature(StreamFeature streamFeature) {
        T value = mFilterPredicate.apply(streamFeature);
        if (value != null) {
            mMatchingItems.add(value);
        }
    }

    /**
     * Called on the main thread call the {@link Consumer} after all removes have been processed.
     */
    public void triggerConsumerUpdate() {
        mConsumer.accept(mMatchingItems);
    }
}
