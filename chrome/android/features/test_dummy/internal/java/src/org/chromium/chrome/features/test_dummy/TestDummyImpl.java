// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.test_dummy;

import android.app.Activity;
import android.content.Intent;
import android.support.annotation.IntDef;
import android.support.v7.app.AlertDialog;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/** Test dummy implementation. */
public class TestDummyImpl implements TestDummy {
    @IntDef({TestCase.EXECUTE_JAVA, TestCase.EXECUTE_NATIVE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface TestCase {
        int EXECUTE_JAVA = 0;
        int EXECUTE_NATIVE = 1;
    }

    @Override
    public void launch(Intent intent, Activity activity) {
        @TestCase
        int testCase = intent.getExtras().getInt("test_case");
        switch (testCase) {
            case TestCase.EXECUTE_JAVA:
                executeJava(activity);
                break;
            case TestCase.EXECUTE_NATIVE:
                executeNative(activity);
                break;
            default:
                throw new RuntimeException("Unknown test case " + testCase);
        }
    }

    private void showDoneDialog(Activity activity, @TestCase int testCase, boolean pass) {
        String message = "Test Case %d: " + (pass ? "pass" : "fail");
        AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        builder.setTitle("Test Dummy Result");
        builder.setMessage(String.format(Locale.US, message, testCase));
        builder.setCancelable(true);
        builder.create().show();
    }

    private void executeJava(Activity activity) {
        showDoneDialog(activity, TestCase.EXECUTE_JAVA, true);
    }

    private void executeNative(Activity activity) {
        boolean result = TestDummySupport.openAndVerifyNativeLibrary();
        showDoneDialog(activity, TestCase.EXECUTE_NATIVE, result);
    }
}
