// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let cls_expectations = [];

cls_expect = (watcher, expectation) => {
  assert_equals(watcher.score, expectation.score);
  if (expectation.score > 0)
    cls_expectations.push(expectation);
};

cls_run_tests = new Promise((resolve, reject) => {
  add_completion_callback((tests, harness_status) => {
    if (harness_status.status != 0) {
      reject(harness_status.message);
      return;
    }
    for (let test of tests) {
      if (test.status != 0 /* PASS */) {
        reject(test.message);
        return;
      }
    }
    resolve(cls_expectations);
  });
});
