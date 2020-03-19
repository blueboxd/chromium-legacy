// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This helper binary can be compiled to aid in development / debugging of the
// QR generation code. It prints a QR code to the console and thus allows much
// faster iteration. It is not built by default, see the BUILD.gn in this
// directory.

#include <stdio.h>

#include <utility>

#include "base/containers/span.h"
#include "chrome/common/qr_code_generator/qr_code_generator.h"

// kTerminalBackgroundIsBright controls the output polarity. Many QR scanners
// will cope with inverted bright / dark but, if you have a bright terminal
// background, you may need to change this.
constexpr bool kTerminalBackgroundIsBright = false;

// kPaint is a pair of UTF-8 encoded code points for U+2588 ("FULL BLOCK").
static constexpr char kPaint[] = "\xe2\x96\x88\xe2\x96\x88";
static constexpr char kNoPaint[] = "  ";

static void PrintHorizontalLine(const char* white) {
  for (size_t x = 0; x < QRCodeGenerator::kSize + 2; x++) {
    fputs(white, stdout);
  }
  fputs("\n", stdout);
}

int main(int argc, char** argv) {
  // Presubmits don't allow fprintf to a variable called |stderr|.
  FILE* const STDERR = stderr;

  if (argc != 2) {
    fprintf(STDERR, "Usage: %s <input string>\n", argv[0]);
    return 1;
  }

  const char* const input = argv[1];
  const size_t input_len = strlen(input);
  if (input_len > QRCodeGenerator::kInputBytes) {
    fprintf(STDERR,
            "Input string too long. Have %u bytes, but max is %u bytes.\n",
            static_cast<unsigned>(input_len),
            static_cast<unsigned>(QRCodeGenerator::kInputBytes));
    return 2;
  }

  const char* black = kNoPaint;
  const char* white = kPaint;
  if (kTerminalBackgroundIsBright) {
    std::swap(black, white);
  }

  QRCodeGenerator generator;
  base::span<const uint8_t, QRCodeGenerator::kTotalSize> code =
      generator.Generate(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(input), input_len));

  PrintHorizontalLine(white);

  size_t i = 0;
  for (size_t y = 0; y < QRCodeGenerator::kSize; y++) {
    fputs(white, stdout);
    for (size_t x = 0; x < QRCodeGenerator::kSize; x++) {
      fputs((code[i++] & 1) ? black : white, stdout);
    }
    fputs(white, stdout);
    fputs("\n", stdout);
  }

  PrintHorizontalLine(white);

  return 0;
}
