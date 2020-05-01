// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/minidump_writer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/base/process_utils.h"
#include "chromecast/crash/linux/crash_util.h"
#include "chromecast/crash/linux/dump_info.h"
#include "chromecast/crash/linux/minidump_generator.h"

namespace chromecast {

namespace {

const char kDumpStateSuffix[] = ".txt.gz";

// Fork and run dumpstate, saving results to minidump_name + ".txt.gz".
int DumpState(const std::string& minidump_name) {
  base::FilePath dumpstate_path;
  if (!CrashUtil::CollectDumpstate(base::FilePath(minidump_name),
                                   &dumpstate_path)) {
    return -1;
  }
  return 0;
}

}  // namespace

MinidumpWriter::MinidumpWriter(MinidumpGenerator* minidump_generator,
                               const std::string& minidump_filename,
                               const MinidumpParams& params,
                               DumpStateCallback dump_state_cb)
    : minidump_generator_(minidump_generator),
      minidump_path_(minidump_filename),
      params_(params),
      dump_state_cb_(std::move(dump_state_cb)) {}

MinidumpWriter::MinidumpWriter(MinidumpGenerator* minidump_generator,
                               const std::string& minidump_filename,
                               const MinidumpParams& params)
    : MinidumpWriter(minidump_generator,
                     minidump_filename,
                     params,
                     base::BindOnce(&DumpState)) {}

MinidumpWriter::~MinidumpWriter() {
}

bool MinidumpWriter::DoWork() {
  // If path is not absolute, append it to |dump_path_|.
  if (!minidump_path_.value().empty() && minidump_path_.value()[0] != '/')
    minidump_path_ = dump_path_.Append(minidump_path_);

  // The path should be a file in the |dump_path_| directory.
  if (dump_path_ != minidump_path_.DirName()) {
    LOG(INFO) << "The absolute path: " << minidump_path_.value() << " is not"
              << "in the correct directory: " << dump_path_.value();
    return false;
  }

  // Generate a minidump at the specified |minidump_path_|.
  if (!minidump_generator_->Generate(minidump_path_.value())) {
    LOG(ERROR) << "Generate minidump failed " << minidump_path_.value();
    return false;
  }

  // Run the dumpstate callback.
  DCHECK(dump_state_cb_);
  if (std::move(dump_state_cb_).Run(minidump_path_.value()) < 0) {
    LOG(ERROR) << "DumpState callback failed.";
    return false;
  }

  // Add this entry to the lockfile.
  const DumpInfo info(minidump_path_.value(),
                      minidump_path_.value() + kDumpStateSuffix,
                      base::Time::Now(), params_);
  if (!AddEntryToLockFile(info)) {
    LOG(ERROR) << "lockfile logging failed";
    return false;
  }

  return true;
}

}  // namespace chromecast
