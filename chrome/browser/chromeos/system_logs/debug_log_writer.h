// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_DEBUG_LOG_WRITER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_DEBUG_LOG_WRITER_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"

namespace chromeos {

// Class for writing logs collected from debugd to a specified location. Also
// supports writing the Chrome user log. Currently used by
// chrome://net-internals#chromeos.
class DebugLogWriter {
 public:
  // Called once StoreDebugLogs is complete. Takes two parameters:
  // - log_path: where the log file was saved in the case of success;
  // - succeeded: was the log file saved successfully.
  typedef base::OnceCallback<void(const base::FilePath& log_path,
                                  bool succeeded)>
      StoreLogsCallback;

  // Stores debug logs in either .tgz or .tar archive (depending on value of
  // |should_compress|) on the |fileshelf|. The file is created on the
  // worker pool, then writing to it is triggered from the UI thread, and
  // finally it is closed (on success) or deleted (on failure) on the worker
  // pool, prior to calling |callback|.
  static void StoreLogs(const base::FilePath& fileshelf,
                        bool should_compress,
                        StoreLogsCallback callback);

  // Stores both system and user logs in .tgz archive on the |fileshelf|.
  static void StoreCombinedLogs(const base::FilePath& fileshelf,
                                StoreLogsCallback callback);

 private:
  DebugLogWriter();
  DISALLOW_COPY_AND_ASSIGN(DebugLogWriter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_DEBUG_LOG_WRITER_H_
