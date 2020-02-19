// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/qr_code_generator/qr_code_generator.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(QRCodeGenerator, Generate) {
  // Without a QR decoder implementation, there's a limit to how much we can
  // test the QR encoder. Therefore this test just runs a generation to ensure
  // that no DCHECKs are hit and that the output has the correct structure. When
  // run under ASan, this will also check that every byte of the output has been
  // written to.
  QRCodeGenerator qr;
  uint8_t input[QRCodeGenerator::kInputBytes];
  memset(input, 'a', sizeof(input));
  auto qr_data = qr.Generate(input);

  int index = 0;
  for (int y = 0; y < QRCodeGenerator::kSize; y++) {
    for (int x = 0; x < QRCodeGenerator::kSize; x++) {
      ASSERT_EQ(0, qr_data[index++] & 0b11111100);
    }
  }
}
