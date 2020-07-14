// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.strictmode;

import android.annotation.TargetApi;
import android.os.StrictMode;
import android.os.StrictMode.ThreadPolicy;
import android.os.strictmode.DiskReadViolation;
import android.os.strictmode.DiskWriteViolation;
import android.os.strictmode.ResourceMismatchViolation;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Consumer;
import org.chromium.base.Function;

import java.util.List;

/**
 * Android P+ {@ThreadStrictModeInterceptor} implementation.
 */
@TargetApi(28)
final class ThreadStrictModeInterceptorP implements ThreadStrictModeInterceptor {
    @NonNull
    private final List<Function<Violation, Integer>> mWhitelistEntries;
    @Nullable
    private final Consumer mCustomPenalty;

    ThreadStrictModeInterceptorP(@NonNull List<Function<Violation, Integer>> whitelistEntries,
            @Nullable Consumer customPenalty) {
        mWhitelistEntries = whitelistEntries;
        mCustomPenalty = customPenalty;
    }

    @Override
    @TargetApi(28)
    public void install(ThreadPolicy threadPolicy) {
        StrictMode.OnThreadViolationListener listener = v -> {
            handleThreadViolation(new Violation(computeType(v), v.getStackTrace()));
        };
        ThreadPolicy.Builder builder = new ThreadPolicy.Builder(threadPolicy);
        builder.penaltyListener(Runnable::run, listener);
        StrictMode.setThreadPolicy(builder.build());
    }

    private void handleThreadViolation(Violation violation) {
        if (violation.isInWhitelist(mWhitelistEntries)) {
            return;
        }
        if (mCustomPenalty != null) {
            mCustomPenalty.accept(violation);
        }
    }

    private static int computeType(android.os.strictmode.Violation violation) {
        if (violation instanceof DiskReadViolation) {
            return Violation.DETECT_DISK_READ;
        } else if (violation instanceof DiskWriteViolation) {
            return Violation.DETECT_DISK_WRITE;
        } else if (violation instanceof ResourceMismatchViolation) {
            return Violation.DETECT_RESOURCE_MISMATCH;
        } else {
            return Violation.DETECT_UNKNOWN;
        }
    }
}
