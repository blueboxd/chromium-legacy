// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class MyClass {
  // Lambdas are backed by a class that may have (depending on what the lambda
  // captures) pointer fields.  The rewriter should ignore such fields (since
  // they don't have an equivalent in source code).
  //
  // No rewrite expected anywhere below.
  void foo() {
    int x = 123;
    auto lambda1 = [this]() -> int { return 123; };
    auto lambda2 = [&]() -> int { return x; };
  }
};
