// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.xsurface.FeedActionsHandler;
import org.chromium.chrome.browser.xsurface.ListContentManager;
import org.chromium.chrome.browser.xsurface.ListContentManagerObserver;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Implementation of ListContentManager that manages a list of feed contents that are supported by
 * either native view or external surface controlled view.
 */
public class FeedListContentManager implements ListContentManager {
    /**
     * Encapsulates the content of an item stored and managed by ListContentManager.
     */
    public abstract static class FeedContent {
        private final String mKey;

        FeedContent(String key) {
            assert key != null && !key.isEmpty();
            mKey = key;
        }

        /**
         * Returns true if the content is supported by the native view.
         */
        public abstract boolean isNativeView();

        /**
         * Returns the key which should uniquely identify the content in the list.
         */
        public String getKey() {
            return mKey;
        }
    }

    /**
     * For the content that is supported by external surface controlled view.
     */
    public static class ExternalViewContent extends FeedContent {
        private final byte[] mData;

        public ExternalViewContent(String key, byte[] data) {
            super(key);
            mData = data;
        }

        /**
         * Returns the raw bytes that are passed to the external surface for rendering if the
         * content is supported by the external surface controlled view.
         */
        public byte[] getBytes() {
            return mData;
        }

        @Override
        public boolean isNativeView() {
            return false;
        }
    }

    /**
     * For the content that is supported by the native view.
     */
    public static class NativeViewContent extends FeedContent {
        private final View mNativeView;

        public NativeViewContent(String key, View nativeView) {
            super(key);
            mNativeView = nativeView;
        }

        /**
         * Returns the native view if the content is supported by it. Null otherwise.
         */
        public View getNativeView() {
            return mNativeView;
        }

        @Override
        public boolean isNativeView() {
            return true;
        }
    }

    private ArrayList<FeedContent> mFeedContentList;
    private ArrayList<ListContentManagerObserver> mObservers;
    private final Map<String, Object> mHandlers;

    FeedListContentManager(
            SurfaceActionsHandler surfaceActionsHandler, FeedActionsHandler feedActionsHandler) {
        mFeedContentList = new ArrayList<FeedContent>();
        mObservers = new ArrayList<ListContentManagerObserver>();
        mHandlers = new HashMap<String, Object>();
        mHandlers.put(SurfaceActionsHandler.KEY, surfaceActionsHandler);
        mHandlers.put(FeedActionsHandler.KEY, feedActionsHandler);
    }

    /**
     * Returns the content at the specified position.
     *
     * @param index The index at which to get the content.
     * @return The content.
     */
    public FeedContent getContent(int index) {
        return mFeedContentList.get(index);
    }

    /**
     * Finds the position of the content with the specified key in the list.
     *
     * @param key The key of the content to search for.
     * @return The position if found, -1 otherwise.
     */
    public int findContentPositionByKey(String key) {
        for (int i = 0; i < mFeedContentList.size(); ++i) {
            if (mFeedContentList.get(i).getKey().equals(key)) {
                return i;
            }
        }
        return -1;
    }

    /**
     * Adds a list of the contents, starting at the specified position.
     *
     * @param index The index at which to insert the first content from the specified collection.
     * @param contents The collection containing contents to be added.
     */
    public void addContents(int index, List<FeedContent> contents) {
        assert index >= 0 && index <= mFeedContentList.size();
        mFeedContentList.addAll(index, contents);
        for (ListContentManagerObserver observer : mObservers) {
            observer.onItemRangeInserted(index, contents.size());
        }
    }

    /**
     * Removes the specified count of contents starting from the speicified position.
     *
     * @param index The index of the first content to be removed.
     * @param count The number of contents to be removed.
     */
    public void removeContents(int index, int count) {
        assert index >= 0 && index < mFeedContentList.size();
        assert index + count <= mFeedContentList.size();
        mFeedContentList.subList(index, index + count).clear();
        for (ListContentManagerObserver observer : mObservers) {
            observer.onItemRangeRemoved(index, count);
        }
    }

    /**
     * Updates a list of the contents, starting at the specified position.
     *
     * @param index The index at which to update the first content from the specified collection.
     * @param contents The collection containing contents to be updated.
     */
    public void updateContents(int index, List<FeedContent> contents) {
        assert index >= 0 && index < mFeedContentList.size();
        assert index + contents.size() <= mFeedContentList.size();
        int pos = index;
        for (FeedContent content : contents) {
            mFeedContentList.set(pos++, content);
        }
        for (ListContentManagerObserver observer : mObservers) {
            observer.onItemRangeChanged(index, contents.size());
        }
    }

    /**
     * Moves the content to a different position.
     *
     * @param curIndex The index of the content to be moved.
     * @param newIndex The new index where the content is being moved to.
     */
    public void moveContent(int curIndex, int newIndex) {
        assert curIndex >= 0 && curIndex < mFeedContentList.size();
        assert newIndex >= 0 && newIndex < mFeedContentList.size();
        int lowIndex;
        int highIndex;
        int distance;
        if (curIndex < newIndex) {
            lowIndex = curIndex;
            highIndex = newIndex;
            distance = -1;
        } else if (curIndex > newIndex) {
            lowIndex = newIndex;
            highIndex = curIndex;
            distance = 1;
        } else {
            return;
        }
        Collections.rotate(mFeedContentList.subList(lowIndex, highIndex + 1), distance);
        for (ListContentManagerObserver observer : mObservers) {
            observer.onItemMoved(curIndex, newIndex);
        }
    }

    @Override
    public boolean isNativeView(int index) {
        return mFeedContentList.get(index).isNativeView();
    }

    @Override
    public byte[] getExternalViewBytes(int index) {
        assert !mFeedContentList.get(index).isNativeView();
        ExternalViewContent externalViewContent = (ExternalViewContent) mFeedContentList.get(index);
        return externalViewContent.getBytes();
    }

    @Override
    public Map<String, Object> getContextValues(int index) {
        return mHandlers;
    }

    @Override
    public View getNativeView(int index, ViewGroup parent) {
        assert mFeedContentList.get(index).isNativeView();
        NativeViewContent nativeViewContent = (NativeViewContent) mFeedContentList.get(index);
        return nativeViewContent.getNativeView();
    }

    @Override
    public void bindNativeView(int index, View v) {
        // TODO(jianli): to be implemented.
    }

    @Override
    public int getItemCount() {
        return mFeedContentList.size();
    }

    @Override
    public void addObserver(ListContentManagerObserver observer) {
        mObservers.add(observer);
    }

    @Override
    public void removeObserver(ListContentManagerObserver observer) {
        mObservers.remove(observer);
    }
}
