// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Handles processing messages in nested run loops.
 *
 * Android does not support nested run loops by default. While running
 * in nested mode, we use reflection to retreive messages from the MessageQueue
 * and dispatch them.
 */
@JNINamespace("content")
public class NestedSystemMessageHandler {
    private static final int QUIT_MESSAGE = 10;
    private static final Handler sHandler = new Handler();
    private static final Method sNextMethod = getMethod(MessageQueue.class, "next");
    private static final Field sMessageTargetField = getField(Message.class, "target");
    private static final Field sMessageFlagsField = getField(Message.class, "flags");

    private static Field getField(Class<?> clazz, String name) {
        Field f = null;
        try {
            f = clazz.getDeclaredField(name);
            f.setAccessible(true);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return f;
    }

    private static Method getMethod(Class<?> clazz, String name) {
        Method m = null;
        try {
            m = clazz.getDeclaredMethod(name);
            m.setAccessible(true);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return m;
    }

    private NestedSystemMessageHandler() {}

    /**
     * Runs a single nested task on the provided MessageQueue
     */
    public static void runSingleNestedLooperTask(MessageQueue queue)
            throws IllegalArgumentException, IllegalAccessException, SecurityException,
                   InvocationTargetException {
        // This call will block if there are no messages in the queue. It will
        // also run or more pending C++ tasks as a side effect before returning
        // |msg|.
        Message msg = (Message) sNextMethod.invoke(queue);
        if (msg == null) return;
        Handler target = (Handler) sMessageTargetField.get(msg);

        if (target != null) {
            target.dispatchMessage(msg);
        }

        // Unset in-use flag.
        Integer oldFlags = (Integer) sMessageFlagsField.get(msg);
        sMessageFlagsField.set(msg, oldFlags & ~(1 << 0 /* FLAG_IN_USE */));

        msg.recycle();
    }

    /**
     * Dispatches the first message from the current MessageQueue, blocking
     * until a task becomes available if the queue is empty. Callbacks for
     * other event handlers registered to the thread's looper (e.g.,
     * MessagePumpAndroid) may also be processed as a side-effect.
     *
     * Returns true if task dispatching succeeded, or false if an exception was
     * thrown.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static boolean dispatchOneMessage() {
        MessageQueue queue = Looper.myQueue();
        try {
            runSingleNestedLooperTask(queue);
        } catch (IllegalArgumentException | IllegalAccessException | SecurityException
                | InvocationTargetException e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    /*
     * Causes a previous call to dispatchOneMessage() to stop blocking and
     * return.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static void postQuitMessage() {
        // Causes MessageQueue.next() to return in case it was blocking waiting
        // for more messages.
        sHandler.sendMessage(sHandler.obtainMessage(QUIT_MESSAGE));
    }
}
