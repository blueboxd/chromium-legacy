// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * We use this annotation to tell which WPR archive file to load for each test cases.
 * Note the archive is a relative path from src/.
 *
 * New tests should also annotate with "WPRRecordReplayTest" in @Feature.
 *
 * For instance, if file_foo is used in test A, file_bar is used
 * in test B.
 *
 *     @Feature("WPRRecordReplayTest")
 *     @WPRArchiveConfigFilePath("file_foo.json")
 *     public void test_A() {
 *        // Write the test case here.
 *     }
 *
 *     @Feature("WPRRecordReplayTest")
 *     @WPRArchiveConfigFilePath("file_bar.json")
 *     public void test_B() {
 *        // Write the test case here.
 *     }
 *
 *  In test_A, the test runner will download file via file_foo.json before running test_A.
 *  The key of the download file is the test unique name.
 *  In test_B, the test runner will download file via file_bar.json before running test_B.
 *  The key of the download file is the test unique name.
 */
@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface WPRArchiveConfigFilePath {
    /**
     * @return one WPRArchiveConfigFilePath.
     */
    public String value();
}
