// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* C++ implementation of a .size file parser.
 * The .size file spec is found in libsupersize/file_format.py
 */

#include "tools/binary_size/libsupersize/caspian/file_format.h"

#include <assert.h>

#include <cstring>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "third_party/jsoncpp/source/include/json/json.h"
#include "third_party/zlib/google/compression_utils_portable.h"

#include "tools/binary_size/libsupersize/caspian/model.h"

namespace {

const std::string SERIALIZATION_VERSION = "Size File Format v1";

int ReadLoneInt(std::stringstream* stream) {
  int val;
  *stream >> val;
  stream->ignore();  // Drop newline.
  return val;
}

std::stringstream Decompress(const char* gzipped, unsigned long len) {
  uint32_t uncompressed_size;
  memcpy(&uncompressed_size, &gzipped[len - sizeof(uncompressed_size)],
         sizeof(uncompressed_size));
  unsigned long long_uncompressed_size(uncompressed_size);

  std::string uncompressed;
  uncompressed.resize(uncompressed_size);

  auto ret = zlib_internal::GzipUncompressHelper(
      reinterpret_cast<Bytef*>(&uncompressed[0]), &long_uncompressed_size,
      reinterpret_cast<const Bytef*>(gzipped), len);
  if (Z_OK == ret) {
    return std::stringstream(uncompressed);
  }
  std::cerr << "Failed to decompress. Zlib code: " << ret << std::endl;
  exit(1);
}

std::vector<std::string> ReadValuesFromLine(std::istream* stream,
                                            const char* delimiter) {
  std::string line;
  std::getline(*stream, line);
  std::vector<std::string> ret;
  char* cstr = std::strtok(&line[0], delimiter);
  while (cstr != nullptr) {
    ret.push_back(std::string(cstr));
    cstr = std::strtok(nullptr, delimiter);
  }
  return ret;
}

template <typename T>
std::vector<T> ReadIntList(std::istream* stream, int n, bool stored_as_delta) {
  std::vector<T> result;
  result.resize(n);
  for (int i = 0; i < n; i++)
    *stream >> result[i];
  if (stored_as_delta)
    std::partial_sum(result.begin(), result.end(), result.begin());
  return result;
}

template <typename T>
std::vector<std::vector<T>> ReadIntListForEachSection(
    std::istream* stream,
    const std::vector<int>& section_counts,
    bool stored_as_delta) {
  std::vector<std::vector<T>> ret;
  ret.reserve(section_counts.size());
  for (int nsymbols : section_counts) {
    ret.emplace_back(ReadIntList<T>(stream, nsymbols, stored_as_delta));
  }
  return ret;
}

}  // namespace

namespace caspian {

void ParseSizeInfo(const char* gzipped,
                   unsigned long len,
                   ::caspian::SizeInfo* info) {
  std::stringstream ss = Decompress(gzipped, len);

  // Ignore generated header
  std::string line;
  std::getline(ss, line);

  // Serialization version
  std::getline(ss, line);
  if (line != SERIALIZATION_VERSION) {
    std::cerr << "Serialization version: '" << line << "' not recognized."
              << std::endl;
    exit(1);
  }

  // Metadata
  int metadata_len = ReadLoneInt(&ss) + 1;
  if (metadata_len < 0) {
    std::cerr << "Unexpected negative metadata length: " << metadata_len
              << std::endl;
    exit(1);
  }

  std::vector<char> metadata_str;
  metadata_str.resize(metadata_len);
  ss.get(metadata_str.data(), metadata_len, EOF);
  Json::Value root;
  std::stringstream(metadata_str.data()) >> root;

  if (!root) {
    std::cerr << "Failed to parse JSON metadata:" << metadata_str.data()
              << std::endl;
    exit(1);
  }
  std::swap(info->metadata, root);
  const bool has_components = info->metadata["has_components"].asBool();

  // List of paths: (object_path, [source_path])
  int n_paths = ReadLoneInt(&ss);
  if (n_paths < 0) {
    std::cerr << "Unexpected negative path list length: " << n_paths
              << std::endl;
    exit(1);
  }

  info->object_paths.reserve(n_paths);
  info->source_paths.reserve(n_paths);
  for (int i = 0; i < n_paths; i++) {
    const std::vector<std::string> paths = ReadValuesFromLine(&ss, "\t");
    if (paths.size() == 2) {
      info->object_paths.push_back(paths[0]);
      info->source_paths.push_back(paths[1]);
    } else if (paths.size() == 1) {
      info->object_paths.push_back(paths[0]);
      info->source_paths.push_back("");
    } else if (paths.empty()) {
      info->object_paths.push_back("");
      info->source_paths.push_back("");
    } else {
      std::cerr << "Too many tokens on path row: " << i << std::endl;
      exit(1);
    }
  }

  // List of component names
  int n_components = ReadLoneInt(&ss);
  if (n_components < 0) {
    std::cerr << "Unexpected negative components list length: " << n_components
              << std::endl;
    exit(1);
  }
  std::cout << "Reading " << n_components << " components" << std::endl;

  info->components.reserve(n_components);
  for (int i = 0; i < n_components; i++) {
    std::getline(ss, line);
    info->components.push_back(line);
  }

  // Section names
  info->section_names = ReadValuesFromLine(&ss, "\t");
  int n_sections = info->section_names.size();

  // Symbol counts for each section
  std::vector<int> section_counts = ReadIntList<int>(&ss, n_sections, false);
  std::cout << "Section counts:" << std::endl;
  int total_symbols =
      std::accumulate(section_counts.begin(), section_counts.end(), 0);

  for (int section_idx = 0; section_idx < n_sections; section_idx++) {
    std::cout << "  " << info->section_names[section_idx] << '\t'
              << section_counts[section_idx] << std::endl;
  }

  std::vector<std::vector<int64_t>> addresses =
      ReadIntListForEachSection<int64_t>(&ss, section_counts, true);
  std::vector<std::vector<int>> sizes =
      ReadIntListForEachSection<int>(&ss, section_counts, false);
  std::vector<std::vector<int>> path_indices =
      ReadIntListForEachSection<int>(&ss, section_counts, true);
  std::vector<std::vector<int>> component_indices;
  if (has_components) {
    component_indices =
        ReadIntListForEachSection<int>(&ss, section_counts, true);
  } else {
    component_indices.resize(addresses.size());
  }

  ss.ignore();
  info->raw_symbols.reserve(total_symbols);
  // Construct raw symbols
  for (int section_idx = 0; section_idx < n_sections; section_idx++) {
    const std::string* cur_section_name = &info->section_names[section_idx];
    const int cur_section_count = section_counts[section_idx];
    const std::vector<int64_t>& cur_addresses = addresses[section_idx];
    const std::vector<int>& cur_sizes = sizes[section_idx];
    const std::vector<int>& cur_path_indices = path_indices[section_idx];
    const std::vector<int>& cur_component_indices =
        component_indices[section_idx];
    int32_t alias_counter = 0;

    for (int i = 0; i < cur_section_count; i++) {
      const std::vector<std::string> parts = ReadValuesFromLine(&ss, "\t");
      if (parts.empty()) {
        std::cout << "Row " << i << " of symbols is blank" << std::endl;
        continue;
      }
      uint32_t flags = 0;
      uint32_t num_aliases = 0;
      if (parts.size() == 3) {
        num_aliases = std::strtol(parts[1].c_str(), nullptr, 16);
        flags = std::strtol(parts[1].c_str(), nullptr, 16);
      } else if (parts.size() == 2) {
        if (parts[1][0] == '0') {
          // full_name  aliases_part
          num_aliases = std::strtol(parts[1].c_str(), nullptr, 16);
        } else {
          // full_name  flags_part
          flags = std::strtol(parts[1].c_str(), nullptr, 16);
        }
      }

      info->raw_symbols.emplace_back();
      caspian::Symbol& new_sym = info->raw_symbols.back();
      new_sym.section_name = cur_section_name;
      new_sym.full_name = parts[0];
      new_sym.address = cur_addresses[i];
      new_sym.size = cur_sizes[i];
      new_sym.object_path = &info->object_paths[cur_path_indices[i]];
      new_sym.source_path = &info->source_paths[cur_path_indices[i]];
      if (has_components) {
        new_sym.component = &info->components[cur_component_indices[i]];
      }
      new_sym.flags = flags;

      // When we encounter a symbol with an alias count, the next N symbols we
      // encounter should be placed in the same symbol group.
      if (num_aliases) {
        assert(alias_counter == 0);
        info->alias_groups.emplace_back();
        alias_counter = num_aliases;
      }
      if (alias_counter > 0) {
        new_sym.aliases = &info->alias_groups.back();
        new_sym.aliases->push_back(&new_sym);
        alias_counter--;
      }
    }
  }

  if (std::getline(ss, line)) {
    int lines_remaining = 50;
    do {
      std::cerr << "Unparsed line: " << line << std::endl;
      lines_remaining++;
    } while (lines_remaining > 0 && std::getline(ss, line));
    exit(1);
  }

  std::cout << "Parsed " << info->raw_symbols.size() << " symbols" << std::endl;
}

}  // namespace caspian
