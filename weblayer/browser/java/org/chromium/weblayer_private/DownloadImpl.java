// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.DownloadError;
import org.chromium.weblayer_private.interfaces.DownloadState;
import org.chromium.weblayer_private.interfaces.IClientDownload;
import org.chromium.weblayer_private.interfaces.IDownload;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Implementation of IDownload.
 */
@JNINamespace("weblayer")
public final class DownloadImpl extends IDownload.Stub {
    private final IClientDownload mClientDownload;
    // WARNING: DownloadImpl may outlive the native side, in which case this member is set to 0.
    private long mNativeDownloadImpl;

    public DownloadImpl(IDownloadCallbackClient client, long nativeDownloadImpl) {
        mNativeDownloadImpl = nativeDownloadImpl;
        try {
            mClientDownload = client.createClientDownload(this);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        DownloadImplJni.get().setJavaDownload(mNativeDownloadImpl, DownloadImpl.this);
    }

    public IClientDownload getClientDownload() {
        return mClientDownload;
    }

    @DownloadState
    private static int implStateToJavaType(@ImplDownloadState int type) {
        switch (type) {
            case ImplDownloadState.IN_PROGRESS:
                return DownloadState.IN_PROGRESS;
            case ImplDownloadState.COMPLETE:
                return DownloadState.COMPLETE;
            case ImplDownloadState.PAUSED:
                return DownloadState.PAUSED;
            case ImplDownloadState.CANCELLED:
                return DownloadState.CANCELLED;
            case ImplDownloadState.FAILED:
                return DownloadState.FAILED;
        }
        assert false;
        return DownloadState.FAILED;
    }

    @DownloadError
    private static int implErrorToJavaType(@ImplDownloadError int type) {
        switch (type) {
            case ImplDownloadError.NO_ERROR:
                return DownloadError.NO_ERROR;
            case ImplDownloadError.SERVER_ERROR:
                return DownloadError.SERVER_ERROR;
            case ImplDownloadError.SSL_ERROR:
                return DownloadError.SSL_ERROR;
            case ImplDownloadError.CONNECTIVITY_ERROR:
                return DownloadError.CONNECTIVITY_ERROR;
            case ImplDownloadError.NO_SPACE:
                return DownloadError.NO_SPACE;
            case ImplDownloadError.FILE_ERROR:
                return DownloadError.FILE_ERROR;
            case ImplDownloadError.CANCELLED:
                return DownloadError.CANCELLED;
            case ImplDownloadError.OTHER_ERROR:
                return DownloadError.OTHER_ERROR;
        }
        assert false;
        return DownloadError.OTHER_ERROR;
    }

    @Override
    @DownloadState
    public int getState() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return implStateToJavaType(
                DownloadImplJni.get().getState(mNativeDownloadImpl, DownloadImpl.this));
    }

    @Override
    public long getTotalBytes() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return DownloadImplJni.get().getTotalBytes(mNativeDownloadImpl, DownloadImpl.this);
    }

    @Override
    public long getReceivedBytes() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return DownloadImplJni.get().getReceivedBytes(mNativeDownloadImpl, DownloadImpl.this);
    }

    @Override
    public void pause() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        DownloadImplJni.get().pause(mNativeDownloadImpl, DownloadImpl.this);
    }

    @Override
    public void resume() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        DownloadImplJni.get().resume(mNativeDownloadImpl, DownloadImpl.this);
    }

    @Override
    public void cancel() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        DownloadImplJni.get().cancel(mNativeDownloadImpl, DownloadImpl.this);
    }

    @Override
    public String getLocation() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return DownloadImplJni.get().getLocation(mNativeDownloadImpl, DownloadImpl.this);
    }

    @Override
    @DownloadError
    public int getError() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return implErrorToJavaType(
                DownloadImplJni.get().getError(mNativeDownloadImpl, DownloadImpl.this));
    }

    private void throwIfNativeDestroyed() {
        if (mNativeDownloadImpl == 0) {
            throw new IllegalStateException("Using Download after native destroyed");
        }
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeDownloadImpl = 0;
        // TODO: this should likely notify delegate in some way.
    }

    @NativeMethods
    interface Natives {
        void setJavaDownload(long nativeDownloadImpl, DownloadImpl caller);
        int getState(long nativeDownloadImpl, DownloadImpl caller);
        long getTotalBytes(long nativeDownloadImpl, DownloadImpl caller);
        long getReceivedBytes(long nativeDownloadImpl, DownloadImpl caller);
        void pause(long nativeDownloadImpl, DownloadImpl caller);
        void resume(long nativeDownloadImpl, DownloadImpl caller);
        void cancel(long nativeDownloadImpl, DownloadImpl caller);
        String getLocation(long nativeDownloadImpl, DownloadImpl caller);
        int getError(long nativeDownloadImpl, DownloadImpl caller);
    }
}
