// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider.internal;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChange;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChange.ChildChanges;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild.Type;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelToken;
import org.chromium.chrome.browser.feed.library.common.Validators;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.NoSuchElementException;

import javax.annotation.concurrent.GuardedBy;

/** Implementation of {@link ModelCursor}. */
public final class ModelCursorImpl implements ModelCursor, Dumpable {
    private static final String TAG = "ModelCursorImpl";

    private final Object mLock = new Object();

    private final String mParentContentId;

    @GuardedBy("mLock")
    private final List<UpdatableModelChild> mChildList;

    @Nullable
    @GuardedBy("mLock")
    private CursorIterator mIterator;

    // #dump() operation counts
    private int mUpdatesAtEnd;
    private int mAppendCount;
    private int mRemoveCount;

    /**
     * Create a new ModelCursorImpl. The {@code childList} needs to be a copy of the original list
     * to prevent {@link java.util.ConcurrentModificationException} for changes to the Model. The
     * cursor is informed of changes through the {@link #updateIterator(FeatureChange
     * featureChang)}.
     */
    public ModelCursorImpl(String parentContentId, List<UpdatableModelChild> childList) {
        this.mParentContentId = parentContentId;
        this.mChildList = new ArrayList<>(childList);
        this.mIterator = new CursorIterator();
    }

    public String getParentContentId() {
        return mParentContentId;
    }

    public void updateIterator(FeatureChange featureChange) {
        // if the state has been released then ignore the change
        if (isAtEnd()) {
            Logger.i(TAG, "Ignoring Update on cursor currently at end");
            mUpdatesAtEnd++;
            return;
        }
        synchronized (mLock) {
            ChildChanges childChanges = featureChange.getChildChanges();
            Logger.i(TAG, "Update Cursor, removes %s, appends %s",
                    childChanges.getRemovedChildren().size(),
                    childChanges.getAppendedChildren().size());

            removeChildren(childChanges.getRemovedChildren());

            for (ModelChild modelChild : featureChange.getChildChanges().getAppendedChildren()) {
                if (modelChild instanceof UpdatableModelChild) {
                    mChildList.add(((UpdatableModelChild) modelChild));
                    mAppendCount++;
                } else {
                    Logger.e(TAG, "non-UpdatableModelChild found, ignored");
                }
            }
        }
    }

    private void removeChildren(List<ModelChild> children) {
        if (children.isEmpty()) {
            return;
        }
        // Remove only needs to remove all children that are beyond the current position because we
        // have visited everything before and can't revisit them with this cursor.
        synchronized (mLock) {
            CursorIterator cursorIterator = Validators.checkNotNull(mIterator);
            int currentPosition = cursorIterator.getPosition();
            List<UpdatableModelChild> realRemoves = new ArrayList<>();
            for (ModelChild modelChild : children) {
                String childKey = modelChild.getContentId();
                // This assumes removes are rare so we can walk the list for each deleted child.
                for (int i = currentPosition; i < mChildList.size(); i++) {
                    UpdatableModelChild child = mChildList.get(i);
                    if (child.getContentId().equals(childKey)) {
                        realRemoves.add(child);
                        break;
                    }
                }
            }
            mRemoveCount += realRemoves.size();
            mChildList.removeAll(realRemoves);
            Logger.i(TAG, "Removed %s children from the Cursor", realRemoves.size());
        }
    }

    @Override
    @Nullable
    public ModelChild getNextItem() {
        // The TimeoutSessionImpl may use the cursor to access the model structure
        ModelChild nextChild;
        synchronized (mLock) {
            if (mIterator == null || !mIterator.hasNext()) {
                release();
                return null;
            }
            nextChild = mIterator.next();
            // If we just hit the last element in the iterator, free all the resources for this
            // cursor.
            if (!mIterator.hasNext()) {
                release();
            }
            // If we have a synthetic token, this is the end of the cursor.
            if (nextChild.getType() == Type.TOKEN) {
                ModelToken token = nextChild.getModelToken();
                if (token instanceof UpdatableModelToken) {
                    UpdatableModelToken updatableModelToken = (UpdatableModelToken) token;
                    if (updatableModelToken.isSynthetic()) {
                        Logger.i(TAG, "Releasing Cursor due to hitting a synthetic token");
                        release();
                    }
                }
            }
        }
        return nextChild;
    }

    /** Release all the state assocated with this cursor */
    public void release() {
        // This could be called on a background thread.
        synchronized (mLock) {
            mIterator = null;
        }
    }

    @Override
    public boolean isAtEnd() {
        synchronized (mLock) {
            return mIterator == null || !this.mIterator.hasNext();
        }
    }

    @VisibleForTesting
    final class CursorIterator implements Iterator<UpdatableModelChild> {
        private int mCursor;

        @Override
        public boolean hasNext() {
            synchronized (mLock) {
                return mCursor < mChildList.size();
            }
        }

        @Override
        public UpdatableModelChild next() {
            synchronized (mLock) {
                if (mCursor >= mChildList.size()) {
                    throw new NoSuchElementException();
                }
                return mChildList.get(mCursor++);
            }
        }

        int getPosition() {
            return mCursor;
        }
    }

    @VisibleForTesting
    List<UpdatableModelChild> getChildListForTesting() {
        synchronized (mLock) {
            return new ArrayList<>(mChildList);
        }
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("atEnd").value(isAtEnd());
        dumper.forKey("updatesPostAtEnd").value(mUpdatesAtEnd).compactPrevious();
        dumper.forKey("appendCount").value(mAppendCount).compactPrevious();
        dumper.forKey("removeCount").value(mRemoveCount).compactPrevious();
    }
}
