// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate;

import java.util.List;

/**
 * Bridge class that lets Android code access native code for feed related functionalities.
 *
 * Created once for each StreamSurfaceMediator corresponding to each NTP/start surface.
 */
@JNINamespace("feed")
public class FeedStreamSurface {
    private final long mNativeFeedStreamSurface;

    /**
     * Creates a {@link FeedStreamSurface} for creating native side bridge to access native feed
     * client implementation.
     */
    public FeedStreamSurface() {
        mNativeFeedStreamSurface = FeedStreamSurfaceJni.get().init(FeedStreamSurface.this);
    }

    /**
     * Called when the stream update content is available. The content will get passed to UI
     */
    @CalledByNative
    void onStreamUpdated(byte[] data) {
        try {
            StreamUpdate streamUpdate = StreamUpdate.parseFrom(data);
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            // Consume exception for now, ignoring unparsable events.
        }
    }

    /**
     * Informs that a particular URL is being navigated to.
     */
    public void navigationDone(String url, boolean inNewTab) {
        FeedStreamSurfaceJni.get().navigationDone(
                mNativeFeedStreamSurface, FeedStreamSurface.this, url, inNewTab);
    }

    /**
     * Requests additional content to be loaded. Once the load is completed, onStreamUpdated will be
     * called.
     */
    public void loadMore() {
        FeedStreamSurfaceJni.get().loadMore(mNativeFeedStreamSurface, FeedStreamSurface.this);
    }

    /**
     * Handles uploading data for ThereAndBackAgain.
     */
    public void processThereAndBackAgain(byte[] data) {
        FeedStreamSurfaceJni.get().processThereAndBackAgain(
                mNativeFeedStreamSurface, FeedStreamSurface.this, data);
    }

    /**
     * Executes an ephemeral change, i.e. dismissals, that is described by a list of data operations
     * serialized as bytes. A change ID will be returned and it can be used to commit or discard the
     * change. If the change ID is 0, it means failure.
     */
    public int executeEphemeralChange(List<byte[]> serializedDataOperations) {
        return FeedStreamSurfaceJni.get().executeEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, serializedDataOperations);
    }

    /**
     * Commits a previously executed ephemeral change.
     */
    public void commitEphemeralChange(int changeId) {
        FeedStreamSurfaceJni.get().commitEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, changeId);
    }

    /**
     * Discards a previously executed ephemeral change.
     */
    public void discardEphemeralChange(int changeId) {
        FeedStreamSurfaceJni.get().discardEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, changeId);
    }

    /**
     * Informs that the surface is opened. We can request the initial set of content now. Once
     * the content is available, onStreamUpdated will be called.
     */
    public void surfaceOpened() {
        FeedStreamSurfaceJni.get().surfaceOpened(mNativeFeedStreamSurface, FeedStreamSurface.this);
    }

    /**
     * Informs that the surface is closed. Any cleanup can be performed now.
     */
    public void surfaceClosed() {
        FeedStreamSurfaceJni.get().surfaceClosed(mNativeFeedStreamSurface, FeedStreamSurface.this);
    }

    @NativeMethods
    interface Natives {
        long init(FeedStreamSurface caller);
        void navigationDone(long nativeFeedStreamSurface, FeedStreamSurface caller, String url,
                boolean inNewTab);
        void loadMore(long nativeFeedStreamSurface, FeedStreamSurface caller);
        void processThereAndBackAgain(
                long nativeFeedStreamSurface, FeedStreamSurface caller, byte[] data);
        int executeEphemeralChange(long nativeFeedStreamSurface, FeedStreamSurface caller,
                List<byte[]> serializedDataOperations);
        void commitEphemeralChange(
                long nativeFeedStreamSurface, FeedStreamSurface caller, int changeId);
        void discardEphemeralChange(
                long nativeFeedStreamSurface, FeedStreamSurface caller, int changeId);
        void surfaceOpened(long nativeFeedStreamSurface, FeedStreamSurface caller);
        void surfaceClosed(long nativeFeedStreamSurface, FeedStreamSurface caller);
    }
}
