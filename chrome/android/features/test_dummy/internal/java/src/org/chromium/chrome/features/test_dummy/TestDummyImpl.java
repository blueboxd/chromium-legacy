// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.test_dummy;

import android.app.Activity;
import android.content.Intent;
import android.support.v7.app.AlertDialog;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/** Test dummy implementation. */
public class TestDummyImpl implements TestDummy {
    @IntDef({TestCase.EXECUTE_JAVA})
    @Retention(RetentionPolicy.SOURCE)
    private @interface TestCase {
        int EXECUTE_JAVA = 0;
    }

    @Override
    public void launch(Intent intent, Activity activity) {
        @TestCase
        int testCase = intent.getExtras().getInt("test_case");
        switch (testCase) {
            case TestCase.EXECUTE_JAVA:
                executeJava(activity);
                break;
            default:
                throw new RuntimeException("Unknown test case " + testCase);
        }
    }

    private void executeJava(Activity activity) {
        showDoneDialog(activity, TestCase.EXECUTE_JAVA);
    }

    private void showDoneDialog(Activity activity, @TestCase int testCase) {
        AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        builder.setTitle("Test Dummy Result");
        builder.setMessage(String.format(Locale.US, "Test Case %d: done", testCase));
        builder.setCancelable(true);
        builder.create().show();
    }
}
