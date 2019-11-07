// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Much of this logic is duplicated at
// tools/binary_size/libsupersize/function_signature.py.

#include <stddef.h>

#include <deque>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "tools/binary_size/libsupersize/caspian/function_signature.h"

namespace caspian {
std::vector<std::string_view> SplitBy(std::string_view str, char delim) {
  std::vector<std::string_view> ret;
  while (true) {
    size_t pos = str.find(delim);
    ret.push_back(str.substr(0, pos));
    if (pos == std::string_view::npos) {
      break;
    }
    str = str.substr(pos + 1);
  }
  return ret;
}

std::tuple<std::string_view, std::string_view, std::string_view> ParseJava(
    std::string_view full_name,
    std::deque<std::string>* owned_strings) {
  std::string maybe_member_type;
  size_t hash_idx = full_name.find('#');
  std::string_view full_class_name;
  std::string_view member;
  std::string_view member_type;
  if (hash_idx != std::string_view::npos) {
    // Parse an already parsed full_name.
    // Format: Class#symbol: type
    full_class_name = full_name.substr(0, hash_idx);
    size_t colon_idx = full_name.find(':');
    member = full_name.substr(hash_idx + 1, colon_idx - hash_idx - 1);
    if (colon_idx != std::string_view::npos) {
      member_type = full_name.substr(colon_idx);
    }
  } else {
    // Format: Class [returntype] functionName()
    std::vector<std::string_view> parts = SplitBy(full_name, ' ');
    full_class_name = parts[0];
    if (parts.size() >= 2) {
      member = parts.back();
    }
    if (parts.size() >= 3) {
      maybe_member_type = ": " + std::string(parts[1]);
      member_type = maybe_member_type;
    }
  }

  std::vector<std::string_view> split = SplitBy(full_class_name, '.');
  std::string_view short_class_name = split.back();

  if (member.empty()) {
    return std::make_tuple(full_name, full_name, short_class_name);
  }
  owned_strings->push_back(std::string(full_class_name) + std::string("#") +
                           std::string(member) + std::string(member_type));
  full_name = owned_strings->back();

  member = member.substr(0, member.find('('));

  owned_strings->push_back(std::string(short_class_name) + std::string("#") +
                           std::string(member));
  std::string_view name = owned_strings->back();

  owned_strings->push_back(std::string(full_class_name) + std::string("#") +
                           std::string(member));
  std::string_view template_name = owned_strings->back();

  return std::make_tuple(full_name, template_name, name);
}
}  // namespace caspian
