// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.LifetimeAssert;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.blink.mojom.MessagePortDescriptor;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.mojo_base.mojom.UnguessableToken;

/**
 * Java wrapper around a native blink::MessagePortDescriptor, and mojom serialized versions of that
 * class (org.chromium.blink.mojom.MessagePortDescriptor).
 *
 * This object is pure implementation detail for AppWebMessagePort. Care must be taken to use this
 * object with something approaching "move" semantics, as under the hood it wraps a Mojo endpoint,
 * which itself has move semantics.
 *
 * The expected usage is as follows:
 *
 *   // Create a pair of descriptors, or rehydrate one from a mojo serialized version.
 *   Pair<AppWebMessagePortDescriptor, AppWebMessagePortDescriptor> pipe =
 *           AppWebMessagePortDescriptor.createPair();
 *   AppWebMessagePortDescriptor port = new AppWebMessagePortDescriptor(
 *           mojomSerializedMessagePortDescriptor);
 *
 *   // Take the handle for use.
 *   MessagePipeHandle handle = port.takeHandleToEntangle();
 *
 *   // Do stuff with the handle.
 *
 *   // Return the handle.
 *   port.givenDisentangledHandle(handle);
 *
 *   // Close the port when we're done with it.
 *   port.close();
 */
public class AppWebMessagePortDescriptor {
    private static final long NATIVE_NULL = 0;
    private static final int INVALID_NATIVE_MOJO_HANDLE = 0; // Mirrors CoreImpl.INVALID_HANDLE.
    private static final long INVALID_SEQUENCE_NUMBER = 0;

    /** Used to ensure that the object is correctly torn down before being finalized. */
    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    /** Handle to the native blink::MessagePortDescriptor associated with this object. */
    private long mNativeMessagePortDescriptor = NATIVE_NULL;
    /**
     * The underlying native handle. When a descriptor is bound to an AppWebMessagePort, handle
     * ownership passes from the native blink::MessagePortDescriptor to a mojo Connector owned by
     * the AppWebMessagePort. Before tearing down, the handle is returned to the native blink
     * component and closed. This will be INVALID_NATIVE_MOJO_HANDLE if the handle is currently
     * owned by the native counterpart, otherwise it is set to the value of the handle that was
     * passed from the native counterpart.
     */
    private int mNativeMojoHandle = INVALID_NATIVE_MOJO_HANDLE;

    /**
     * @return a newly created pair of connected AppWebMessagePortDescriptors.
     */
    static Pair<AppWebMessagePortDescriptor, AppWebMessagePortDescriptor> createPair() {
        long[] nativeMessagePortDescriptors = AppWebMessagePortDescriptorJni.get().createPair();
        assert nativeMessagePortDescriptors.length
                == 2 : "native createPair returned an invalid value";
        Pair<AppWebMessagePortDescriptor, AppWebMessagePortDescriptor> pair =
                new Pair<>(new AppWebMessagePortDescriptor(nativeMessagePortDescriptors[0]),
                        new AppWebMessagePortDescriptor(nativeMessagePortDescriptors[1]));
        return pair;
    }

    /**
     * Creates an instance of a descriptor from a mojo-serialized version.
     */
    AppWebMessagePortDescriptor(MessagePortDescriptor blinkMessagePortDescriptor) {
        // If the descriptor is invalid then immediately go to an invalid state, which also means
        // we're safe to GC.
        if (!isBlinkMessagePortDescriptorValid(blinkMessagePortDescriptor)) {
            reset();
            return;
        }
        mNativeMessagePortDescriptor = AppWebMessagePortDescriptorJni.get().create(
                blinkMessagePortDescriptor.pipeHandle.releaseNativeHandle(),
                blinkMessagePortDescriptor.id.low, blinkMessagePortDescriptor.id.high,
                blinkMessagePortDescriptor.sequenceNumber);
        resetBlinkMessagePortDescriptor(blinkMessagePortDescriptor);
    }

    /**
     * @return true if this descriptor is valid (has an active native counterpart). Safe to call
     *         anytime.
     */
    boolean isValid() {
        return mNativeMessagePortDescriptor != NATIVE_NULL;
    }

    /**
     * @return the MessagePipeHandle corresponding to this descriptors mojo endpoint. This must be
     *         returned to the descriptor before it is closed, and before it is finalized. It is
     *         valid to take and return the handle multiple times. This is meant to be called in
     *         order to entangle the handle with a mojo.system.Connector.
     */
    MessagePipeHandle takeHandleToEntangle() {
        ensureNativeMessagePortDescriptorExists();
        assert mNativeMojoHandle == INVALID_NATIVE_MOJO_HANDLE : "handle already taken";
        mNativeMojoHandle = AppWebMessagePortDescriptorJni.get().takeHandleToEntangle(
                mNativeMessagePortDescriptor);
        assert mNativeMojoHandle
                != INVALID_NATIVE_MOJO_HANDLE : "native object returned an invalid handle";
        return wrapNativeHandle(mNativeMojoHandle);
    }

    /**
     * @return true if the port is currently entangled, false otherwise. Safe to call anytime.
     */
    boolean isEntangled() {
        return mNativeMessagePortDescriptor != NATIVE_NULL
                && mNativeMojoHandle != INVALID_NATIVE_MOJO_HANDLE;
    }

    /**
     * Returns the mojo endpoint to this descriptor, previously vended out by
     * "takeHandleToEntangle". If vended out the handle must be returned prior to closing this
     * object, which must be done prior to the object being destroyed. It is valid to take and
     * return the handle multiple times.
     */
    void giveDisentangledHandle(MessagePipeHandle handle) {
        ensureNativeMessagePortDescriptorExists();
        assert mNativeMojoHandle != INVALID_NATIVE_MOJO_HANDLE : "handle not currently taken";
        int nativeHandle = handle.releaseNativeHandle();
        assert nativeHandle
                == mNativeMojoHandle : "returned handle does not match the taken handle";
        AppWebMessagePortDescriptorJni.get().giveDisentangledHandle(
                mNativeMessagePortDescriptor, mNativeMojoHandle);
        mNativeMojoHandle = INVALID_NATIVE_MOJO_HANDLE;
    }

    /**
     * Closes this descriptor. This will cause the native counterpart to be destroyed, and the
     * descriptor will no longer be valid after this point. It is safe to call this repeatedly. It
     * is illegal to close a descriptor whose handle is presently taken by a corresponding
     * AppWebMessagePort.
     */
    void close() {
        if (mNativeMessagePortDescriptor == NATIVE_NULL) return;
        assert mNativeMojoHandle
                == INVALID_NATIVE_MOJO_HANDLE : "closing a descriptor whose handle is taken";
        AppWebMessagePortDescriptorJni.get().closeAndDestroy(mNativeMessagePortDescriptor);
        reset();
    }

    /**
     * Passes ownership of this descriptor into a blink.mojom.MessagePortDescriptor. This allows the
     * port to be transferred as part of a TransferableMessage. Invalidates this object in the
     * process.
     */
    MessagePortDescriptor passAsBlinkMojomMessagePortDescriptor() {
        ensureNativeMessagePortDescriptorExists();
        assert mNativeMojoHandle
                == INVALID_NATIVE_MOJO_HANDLE : "passing a descriptor whose handle is taken";

        // This is in the format [nativeHandle, idLow, idHigh, sequenceNumber].
        long[] serialized =
                AppWebMessagePortDescriptorJni.get().passSerialized(mNativeMessagePortDescriptor);
        assert serialized.length == 4 : "native passSerialized returned an invalid value";

        int nativeHandle = (int) serialized[0];
        long idLow = serialized[1];
        long idHigh = serialized[2];
        long sequenceNumber = serialized[3];

        MessagePortDescriptor port = new MessagePortDescriptor();
        port.pipeHandle = wrapNativeHandle(nativeHandle);
        port.id = new UnguessableToken();
        port.id.low = idLow;
        port.id.high = idHigh;
        port.sequenceNumber = sequenceNumber;

        reset();

        return port;
    }

    /**
     * Private constructor used in creating a matching pair of descriptors.
     */
    private AppWebMessagePortDescriptor(long nativeMessagePortDescriptor) {
        assert nativeMessagePortDescriptor != NATIVE_NULL : "invalid nativeMessagePortDescriptor";
        mNativeMessagePortDescriptor = nativeMessagePortDescriptor;
    }

    /**
     * Helper function that throws an exception if the native counterpart does not exist.
     */
    private void ensureNativeMessagePortDescriptorExists() {
        assert mNativeMessagePortDescriptor != NATIVE_NULL : "native descriptor does not exist";
    }

    /**
     * Resets this object. Assumes the native side has already been appropriately torn down.
     */
    private void reset() {
        mNativeMessagePortDescriptor = NATIVE_NULL;
        mNativeMojoHandle = INVALID_NATIVE_MOJO_HANDLE;
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    /**
     * @return true if the provided blink.mojom.MessagePortDescriptor is valid.
     */
    private static boolean isBlinkMessagePortDescriptorValid(
            MessagePortDescriptor blinkMessagePortDescriptor) {
        if (!blinkMessagePortDescriptor.pipeHandle.isValid()) {
            return false;
        }
        if (blinkMessagePortDescriptor.id == null) {
            return false;
        }
        if (blinkMessagePortDescriptor.id.low == 0 && blinkMessagePortDescriptor.id.high == 0) {
            return false;
        }
        if (blinkMessagePortDescriptor.sequenceNumber == INVALID_SEQUENCE_NUMBER) {
            return false;
        }
        return true;
    }

    /**
     * Resets the provided blink.mojom.MessagePortDescriptor.
     */
    private static void resetBlinkMessagePortDescriptor(
            MessagePortDescriptor blinkMessagePortDescriptor) {
        blinkMessagePortDescriptor.pipeHandle.close();
        if (blinkMessagePortDescriptor.id != null) {
            blinkMessagePortDescriptor.id.low = 0;
            blinkMessagePortDescriptor.id.high = 0;
        }
        blinkMessagePortDescriptor.sequenceNumber = 0;
    }

    /**
     * Wraps the provided native handle as MessagePipeHandle.
     */
    MessagePipeHandle wrapNativeHandle(int nativeHandle) {
        return CoreImpl.getInstance().acquireNativeHandle(nativeHandle).toMessagePipeHandle();
    }

    @NativeMethods
    interface Native {
        long[] createPair();
        long create(int nativeHandle, long idLow, long idHigh, long sequenceNumber);
        // Deliberately do not use nativeObjectType naming to avoid automatic typecasting and
        // member function dispatch; these need to be routed to static functions.
        int takeHandleToEntangle(long blinkMessagePortDescriptor);
        void giveDisentangledHandle(long blinkMessagePortDescriptor, int nativeHandle);
        long[] passSerialized(long blinkMessagePortDescriptor);
        void closeAndDestroy(long blinkMessagePortDescriptor);
    }
}
