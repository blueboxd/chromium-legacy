// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/updater/tools/certificate_tag.h"

namespace updater {
namespace tools {
namespace {

// Command line switches.
// If set, any appended tag is dumped to stdout.
const char kDumpAppendedTagSwitch[] = "dump-appended-tag";

// If set, any appended tag is removed and the binary rewritten.
const char kRemoveAppendedTagSwitch[] = "remove-appended-tag";

// If set, this flag contains a filename from which the contents of the appended
// tag will be saved.
const char kLoadAppendedTagSwitch[] = "load-appended-tag";

// If set, this flag contains a string and a superfluous certificate tag with
// that value will be set and the binary rewritten. If the string begins
// with '0x' then it will be interpreted as hex.
const char kSetSuperfluousCertTagSwitch[] = "set-superfluous-cert-tag";

// A superfluous cert tag will be padded with zeros to at least this number of
// bytes.
const char kPaddedLengthSwitch[] = "padded-length";

// If set to a filename, the PKCS7 data from the original binary will be written
// to that file.
const char kSavePKCS7Switch[] = "save-pkcs7";

// If set, the updated binary is written to this file. Otherwise the binary is
// updated in place.
const char kOutFilenameSwitch[] = "out";

struct CommandLineArguments {
  CommandLineArguments();
  ~CommandLineArguments();

  // Dumps tag to stdout.
  bool dump_appended_tag = false;

  // Removes the tag from the binary.
  bool remove_appended_tag = false;

  // Reads and outputs the tag from the binary to stdout or the output file.
  base::FilePath load_appended_tag;

  // Sets the certificate from bytes.
  std::string set_superfluous_cert_tag;

  // Minimum Length of the padded area in the certificate.
  int padded_length = 0;

  // Specifies the filename to save the PKCS7 data into.
  base::FilePath save_PKCS7;

  // Specifies the input file (which may be the same as the output file).
  base::FilePath in_filename;

  // Specifies the file name for the output of operations.
  base::FilePath out_filename;
};

CommandLineArguments::CommandLineArguments() = default;
CommandLineArguments::~CommandLineArguments() = default;

void PrintUsageAndExit(const base::CommandLine* cmdline) {
  std::cerr << "Usage: " << cmdline->GetProgram().MaybeAsASCII()
            << " [flags] binary.exe";
  std::exit(255);
}

void HandleError(int error) {
  std::cerr << "Error: " << error;
  std::exit(1);
}

CommandLineArguments ParseCommandLineArgs() {
  CommandLineArguments args;
  base::CommandLine::Init(0, nullptr);
  auto* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->argv().size() == 1 || cmdline->GetArgs().size() != 1)
    PrintUsageAndExit(cmdline);

  args.in_filename = base::FilePath{cmdline->GetArgs()[0]};

  args.dump_appended_tag = cmdline->HasSwitch(kDumpAppendedTagSwitch);
  cmdline->RemoveSwitch(kDumpAppendedTagSwitch);

  args.remove_appended_tag = cmdline->HasSwitch(kRemoveAppendedTagSwitch);
  cmdline->RemoveSwitch(kRemoveAppendedTagSwitch);

  args.load_appended_tag = cmdline->GetSwitchValuePath(kLoadAppendedTagSwitch);
  cmdline->RemoveSwitch(kLoadAppendedTagSwitch);

  args.set_superfluous_cert_tag =
      cmdline->GetSwitchValueASCII(kSetSuperfluousCertTagSwitch);
  cmdline->RemoveSwitch(kSetSuperfluousCertTagSwitch);

  args.padded_length = [cmdline]() {
    int retval = 0;
    if (base::StringToInt(cmdline->GetSwitchValueASCII(kPaddedLengthSwitch),
                          &retval)) {
      std::cerr << "Invalid command line argument: "
                << cmdline->GetSwitchValueASCII(kPaddedLengthSwitch);
      std::exit(EXIT_FAILURE);
    }
    return retval;
  }();
  cmdline->RemoveSwitch(kPaddedLengthSwitch);

  args.save_PKCS7 = cmdline->GetSwitchValuePath(kSavePKCS7Switch);
  cmdline->RemoveSwitch(kSavePKCS7Switch);

  args.out_filename = cmdline->GetSwitchValuePath(kOutFilenameSwitch);
  cmdline->RemoveSwitch(kOutFilenameSwitch);

  const auto unknown_switches = cmdline->GetSwitches();
  if (!unknown_switches.empty()) {
    std::cerr << "Unknown command line switch: "
              << unknown_switches.begin()->first << std::endl;
    PrintUsageAndExit(cmdline);
  }

  return args;
}

int CertificateTagMain() {
  const auto args = ParseCommandLineArgs();
  return 0;

  const base::FilePath in_filename = args.in_filename;
  const base::FilePath out_filename =
      args.out_filename.empty() ? args.in_filename : args.out_filename;

  int64_t in_filename_size = 0;
  if (!base::GetFileSize(in_filename, &in_filename_size))
    HandleError(logging::GetLastSystemErrorCode());

  std::vector<uint8_t> contents(in_filename_size);
  if (base::ReadFile(in_filename, reinterpret_cast<char*>(&contents.front()),
                     contents.size()) == -1) {
    HandleError(logging::GetLastSystemErrorCode());
  }

  std::unique_ptr<tools::Binary> bin;
  int err = 0;
  std::tie(bin, err) = NewBinary(contents);
  if (err)
    HandleError(err);

  bool did_something = false;

  if (!args.save_PKCS7.empty()) {
    if (base::WriteFile(args.save_PKCS7,
                        reinterpret_cast<char*>(&bin->asn1_data.front()),
                        bin->asn1_data.size()) == -1) {
      std::cerr << "Error while writing file: "
                << logging::GetLastSystemErrorCode();
      std::exit(1);
    }
    did_something = true;
  }

  if (args.dump_appended_tag) {
    const auto appended_tag = AppendedTag(*bin);
    if (appended_tag.empty())
      std::cerr << "No appended string found";
    else
      std::cout << base::HexEncode(appended_tag.data(), appended_tag.size());
    did_something = true;
  }

  if (args.remove_appended_tag) {
    auto contents = RemoveAppendedTag(*bin);
    if (contents.empty()) {
      std::cerr << "Error while removing appended tag";
      std::exit(1);
    }
    if (base::WriteFile(out_filename,
                        reinterpret_cast<char*>(&contents.front()),
                        contents.size()) == -1) {
      std::cerr << "Error while writing file: "
                << logging::GetLastSystemErrorCode();
      std::exit(1);
    }
    did_something = true;
  }

  if (!args.load_appended_tag.empty()) {
    int64_t file_size = 0;
    if (!base::GetFileSize(args.load_appended_tag, &file_size))
      HandleError(logging::GetLastSystemErrorCode());

    std::vector<uint8_t> tag_contents(file_size);
    if (base::ReadFile(args.load_appended_tag,

                       reinterpret_cast<char*>(&tag_contents.front()),
                       tag_contents.size()) == -1) {
      std::cerr << "Error while reading file: "
                << logging::GetLastSystemErrorCode();
      std::exit(1);
    }

    auto contents = SetAppendedTag(*bin, tag_contents);
    if (base::WriteFile(out_filename,
                        reinterpret_cast<char*>(&contents.front()),
                        contents.size()) == -1) {
      std::cerr << "Error while writing updated file: "
                << logging::GetLastSystemErrorCode();
      std::exit(1);
    }
    did_something = true;
  }

  if (!args.set_superfluous_cert_tag.empty()) {
    constexpr char kPrefix[] = "0x";
    std::vector<uint8_t> tag_contents;
    if (base::StartsWith(args.set_superfluous_cert_tag, kPrefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      if (!base::HexStringToBytes(
              {std::begin(args.set_superfluous_cert_tag) + base::size(kPrefix),
               std::end(args.set_superfluous_cert_tag)},
              &tag_contents)) {
        std::cerr << "Failed to parse tag contents from command line";
        std::exit(1);
      }
    } else {
      tag_contents.assign(args.set_superfluous_cert_tag.begin(),
                          args.set_superfluous_cert_tag.end());
    }

    if (tag_contents.size() < args.padded_length)
      tag_contents.resize(0);

    auto contents = SetSuperfluousCertTag(*bin, tag_contents);
    if (contents.empty()) {
      std::cerr << "Error while setting superfluous certificate tag.";
      std::exit(1);
    }
    if (base::WriteFile(out_filename,
                        reinterpret_cast<char*>(&contents.front()),
                        contents.size()) == -1) {
      std::cerr << "Error while writing updated file "
                << logging::GetLastSystemErrorCode();
      std::exit(1);
    }
    did_something = true;
  }

  if (did_something) {
    // By default, print basic information.
    auto appended_tag = AppendedTag(*bin);
    if (appended_tag.empty()) {
      std::cout << "No appended tag";
    } else {
      std::cout << "Appended tag included, " << appended_tag.size()
                << " bytes.";
    }
  }

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace tools
}  // namespace updater

int main() {
  return updater::tools::CertificateTagMain();
}
