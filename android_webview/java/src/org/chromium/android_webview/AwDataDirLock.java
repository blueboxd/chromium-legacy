// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.os.Build;
import android.os.Process;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;

/**
 * Handles locking the WebView's data directory, to prevent concurrent use from
 * more than one process.
 */
abstract class AwDataDirLock {
    private static final String TAG = "AwDataDirLock";

    private static final String EXCLUSIVE_LOCK_FILE = "webview_data.lock";
    private static final int LOCK_RETRIES = 5;
    private static final int LOCK_SLEEP_MS = 100;

    private static RandomAccessFile sLockFile;
    private static FileLock sExclusiveFileLock;

    static void lock(final Context appContext) {
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped("AwDataDirLock.lock");
                StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            if (sExclusiveFileLock != null) {
                // We have already called lock() and successfully acquired the lock in this process.
                // This shouldn't happen, but may be the result of an app catching an exception
                // thrown during initialization and discarding it, causing us to later attempt to
                // initialize WebView again. By continuing, we are racing with the file being closed
                // by the finalizer (releasing the lock), and so we may or may not crash with
                // OverlappingFileLockException. Ideally we'd explicitly crash here but we don't
                // want to make this problem worse until we fully understand it, so on stable we
                // just log and then let execution continue; the app will either be lucky or not
                // with roughly the same chance/conditions as before these checks were implemented.
                throwIfNotStableChannel("Data directory already locked by current process - "
                        + "https://crbug.com/1054774");
            } else if (sLockFile != null) {
                // We have already called lock() but didn't succeed in getting the lock. As above
                // this shouldn't happen, but this won't lead to OverlappingFileLockException.
                // Again, we just log and then let execution continue on stable; if the app fails to
                // acquire the lock again we'll crash from that anyway.
                throwIfNotStableChannel("Previous data directory lock attempt in current process - "
                        + "https://crbug.com/1054744");
            }

            String dataPath = PathUtils.getDataDirectory();
            File lockFile = new File(dataPath, EXCLUSIVE_LOCK_FILE);

            try {
                // Note that the file is kept open intentionally.
                sLockFile = new RandomAccessFile(lockFile, "rw");
            } catch (IOException e) {
                // Failing to create the lock file is always fatal; even if multiple processes are
                // using the same data directory we should always be able to access the file itself.
                throw new RuntimeException("Failed to create lock file " + lockFile, e);
            }

            // Some Android versions may have a race where a new instance of an app process can
            // be started while an existing one is still in the process of being killed. Retry
            // the lock a few times to give the old process time to fully go away.
            for (int attempts = 1; attempts <= LOCK_RETRIES; ++attempts) {
                try {
                    sExclusiveFileLock = sLockFile.getChannel().tryLock();
                } catch (IOException e) {
                    // Older versions of Android incorrectly throw IOException when the flock()
                    // call fails with EAGAIN, instead of returning null. Just ignore it.
                }
                if (sExclusiveFileLock != null) {
                    // We got the lock; write out info for debugging.
                    writeCurrentProcessInfo(sLockFile);
                    recordLockAttempts(attempts);
                    return;
                }

                // If we're not out of retries, sleep and try again.
                if (attempts == LOCK_RETRIES) break;
                try {
                    Thread.sleep(LOCK_SLEEP_MS);
                } catch (InterruptedException e) {
                }
            }

            // We failed to get the lock even after retrying.
            // Many existing apps rely on this even though it's known to be unsafe.
            // Make it fatal when on P for apps that target P or higher
            String error = getLockFailureReason(sLockFile);
            boolean dieOnFailure = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                    && appContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.P;
            if (dieOnFailure) {
                throw new RuntimeException(error);
            } else {
                Log.w(TAG, error);
                // Record an attempt count of 0 to indicate that we proceeded without the lock.
                recordLockAttempts(0);
            }
        }
    }

    private static void writeCurrentProcessInfo(final RandomAccessFile file) {
        try {
            // Truncate the file first to get rid of old data.
            file.setLength(0);
            file.writeInt(Process.myPid());
            file.writeUTF(ContextUtils.getProcessName());
        } catch (IOException e) {
            // Don't crash just because something failed here, as it's only for debugging.
            Log.w(TAG, "Failed to write info to lock file", e);
        }
    }

    private static void recordLockAttempts(int attempts) {
        // We log values from [0, LOCK_RETRIES]. Histogram samples are expected to be [0, max).
        // 0 just goes to the underflow bucket, so min=1 and max=LOCK_RETRIES+1.
        // To get bucket width 1, buckets must be max-min+2
        RecordHistogram.recordLinearCountHistogram("Android.WebView.Startup.DataDirLockAttempts",
                attempts, 1, LOCK_RETRIES + 1, LOCK_RETRIES + 2);
    }

    private static String getLockFailureReason(final RandomAccessFile file) {
        final StringBuilder error = new StringBuilder("Using WebView from more than one process at "
                + "once with the same data directory is not supported. https://crbug.com/558377 "
                + ": Current process ");
        error.append(ContextUtils.getProcessName());
        error.append(" (pid ").append(Process.myPid()).append("), lock owner ");
        try {
            int pid = file.readInt();
            String processName = file.readUTF();
            error.append(processName).append(" (pid ").append(pid).append(")");

            // Check the status of the pid holding the lock by sending it a null signal.
            // This doesn't actually send a signal, just runs the kernel access checks.
            try {
                Os.kill(pid, 0);

                // No exception means the process exists and has the same uid as us, so is
                // probably an instance of the same app. Leave the message alone.
            } catch (ErrnoException e) {
                if (e.errno == OsConstants.ESRCH) {
                    // pid did not exist - the lock should have been released by the kernel,
                    // so this process info is probably wrong.
                    error.append(" doesn't exist!");
                } else if (e.errno == OsConstants.EPERM) {
                    // pid existed but didn't have the same uid as us.
                    // Most likely the pid has just been recycled for a new process
                    error.append(" pid has been reused!");
                } else {
                    // EINVAL is the only other documented return value for kill(2) and should never
                    // happen for signal 0, so just complain generally.
                    error.append(" status unknown!");
                }
            }
        } catch (IOException e) {
            // We'll get IOException if we failed to read the pid and process name; e.g. if the
            // lockfile is from an old version of WebView or an IO error occurred somewhere.
            error.append(" unknown");
        }
        return error.toString();
    }

    private static void throwIfNotStableChannel(String error) {
        if (VersionConstants.CHANNEL == Channel.STABLE) {
            Log.e(TAG, error);
        } else {
            throw new RuntimeException(error);
        }
    }
}
