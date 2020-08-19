// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_TEST_UTILS_H_
#define PDF_TEST_TEST_UTILS_H_

namespace pp {
class FloatRect;
class Rect;
}  // namespace pp

namespace chrome_pdf {

void CompareRect(const pp::Rect& expected_rect, const pp::Rect& given_rect);
void CompareRect(const pp::FloatRect& expected_rect,
                 const pp::FloatRect& given_rect);

}  // namespace chrome_pdf

#endif  // PDF_TEST_TEST_UTILS_H_