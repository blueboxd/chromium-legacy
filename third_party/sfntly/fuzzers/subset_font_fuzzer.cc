// Copyright 2016 The Chromimum Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "third_party/libFuzzer/src/utils/FuzzedDataProvider.h"
#include "third_party/sfntly/src/cpp/src/sample/chromium/font_subsetter.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr int kMaxFontNameSize = 128;
  constexpr int kMaxFontSize = 50 * 1024 * 1024;
  FuzzedDataProvider fuzzed_data(data, size);

  size_t font_name_size =
      fuzzed_data.ConsumeIntegralInRange(0, kMaxFontNameSize);
  std::string font_name = fuzzed_data.ConsumeBytesAsString(font_name_size);

  size_t font_str_size = fuzzed_data.ConsumeIntegralInRange(0, kMaxFontSize);
  std::vector<unsigned char> font_str =
      fuzzed_data.ConsumeBytes<unsigned char>(font_str_size);

  std::vector<uint8_t> glyph_ids_str =
      fuzzed_data.ConsumeRemainingBytes<uint8_t>();
  const unsigned int* glyph_ids =
      reinterpret_cast<const unsigned int*>(glyph_ids_str.data());
  size_t glyph_ids_size = glyph_ids_str.size() *
                          sizeof(decltype(glyph_ids_str)::value_type) /
                          sizeof(*glyph_ids);

  unsigned char* output = nullptr;
  SfntlyWrapper::SubsetFont(font_name.c_str(), font_str.data(), font_str.size(),
                            glyph_ids, glyph_ids_size, &output);
  delete[] output;
  return 0;
}
