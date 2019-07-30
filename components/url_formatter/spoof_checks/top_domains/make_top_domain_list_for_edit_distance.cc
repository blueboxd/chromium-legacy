// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This binary generates an array of top domains suitable for edit distance
// matching. The input is the list of (skeleton, domain) pairs. The output is
// written as a C array of the domains.

#include <iostream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/uspoof.h"

namespace {

const size_t kTopN = 500;

void PrintHelp() {
  std::cout << "make_top_domain_list_for_edit_distance <input-file>"
            << " <output-file> [--v=1]" << std::endl;
}

std::string GetSkeleton(const std::string& domain,
                        const USpoofChecker* spoof_checker) {
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString ustr_skeleton;
  uspoof_getSkeletonUnicodeString(spoof_checker, 0 /* not used */,
                                  icu::UnicodeString::fromUTF8(domain),
                                  ustr_skeleton, &status);
  std::string skeleton;
  return U_SUCCESS(status) ? ustr_skeleton.toUTF8String(skeleton) : skeleton;
}

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

#if defined(OS_WIN)
  std::vector<std::string> args;
  base::CommandLine::StringVector wide_args = command_line.GetArgs();
  for (const auto& arg : wide_args) {
    args.push_back(base::WideToUTF8(arg));
  }
#else
  base::CommandLine::StringVector args = command_line.GetArgs();
#endif
  if (args.size() < 2) {
    PrintHelp();
    return 1;
  }

  base::FilePath input_path =
      base::MakeAbsoluteFilePath(base::FilePath::FromUTF8Unsafe(argv[1]));
  if (!base::PathExists(input_path)) {
    LOG(ERROR) << "Input path doesn't exist: " << input_path;
    return 1;
  }

  std::string input_text;
  if (!base::ReadFileToString(input_path, &input_text)) {
    LOG(ERROR) << "Could not read input file: " << input_path;
    return 1;
  }

  std::vector<std::string> lines = base::SplitString(
      input_text, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  base::i18n::InitializeICU();
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<USpoofChecker, decltype(&uspoof_close)> spoof_checker(
      uspoof_open(&status), &uspoof_close);
  if (U_FAILURE(status)) {
    std::cerr << "Failed to create an ICU uspoof_checker due to "
              << u_errorName(status) << ".\n";
    return 1;
  }

  size_t count = 0;
  std::string output =
      R"(#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
namespace top500_domains {
const char* const kTop500[500] = {
)";
  for (std::string line : lines) {
    base::TrimWhitespaceASCII(line, base::TRIM_ALL, &line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    if (count >= kTopN)
      break;
    if (!url_formatter::top_domains::IsEditDistanceCandidate(line)) {
      continue;
    }
    count++;
    const std::string skeleton = GetSkeleton(line, spoof_checker.get());
    output += "\"" + skeleton + "\",\n";
  }
  output += R"(};
}  // namespace top500_domains
)";

  base::FilePath output_path = base::FilePath::FromUTF8Unsafe(argv[2]);
  if (base::WriteFile(output_path, output.c_str(),
                      static_cast<uint32_t>(output.size())) <= 0) {
    LOG(ERROR) << "Failed to write output: " << output_path;
    return 1;
  }
  return 0;
}
