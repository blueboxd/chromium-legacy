// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_FUNCTION_SIGNATURE_H_
#define TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_FUNCTION_SIGNATURE_H_

#include <deque>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace caspian {
std::vector<std::string_view> SplitBy(std::string_view str, char delim);

// Breaks Java |full_name| into parts.
// If needed, new strings are allocated into |owned_strings|.
// Returns: A tuple of (full_name, template_name, name), where:
//   * full_name = "class_with_package#member(args): type"
//   * template_name = "class_with_package#member"
//   * name = "class_without_package#member"
std::tuple<std::string_view, std::string_view, std::string_view> ParseJava(
    std::string_view full_name,
    std::deque<std::string>* owned_strings);
}  // namespace caspian

#endif  // TOOLS_BINARY_SIZE_LIBSUPERSIZE_CASPIAN_FUNCTION_SIGNATURE_H_
