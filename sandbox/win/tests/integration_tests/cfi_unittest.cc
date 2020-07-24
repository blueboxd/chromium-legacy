// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <intrin.h>

#include <windows.h>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// ASLR must be enabled for CFG to be enabled, and ASLR is disabled in debug
// builds.
// Test cannot run with ASAN builds as CFI linker is currently disabled because
// of perf issues caused by https://crbug.com/846966.
#if !defined(_DEBUG) && !defined(ADDRESS_SANITIZER)

namespace {

DWORD CALLBACK CopyProgressRoutine(LARGE_INTEGER total_file_size,
                                   LARGE_INTEGER total_bytes_transferred,
                                   LARGE_INTEGER stream_size,
                                   LARGE_INTEGER stream_bytes_transferred,
                                   DWORD stream_number,
                                   DWORD callback_reason,
                                   HANDLE source_file,
                                   HANDLE destination_file,
                                   LPVOID context) {
  __asm {
     nop
     nop
     ret
  }
  return PROGRESS_CONTINUE;
}

}  // namespace

// Make sure Microsoft binaries compiled with CFG cannot call indirect pointers
// not listed in the loader config for this test binary.
TEST(CFGSupportTests, MsIndirectFailure) {
  // CFG is only supported on >= Win8.1 Update 3.
  // Not checking for update, since test infra is updated and it would add
  // a lot of complexity.
  if (base::win::GetVersion() < base::win::Version::WIN8_1)
    return;

  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &exe_path));

  using ProcessCallbackRoutineType = decltype(&CopyProgressRoutine);

  // Create a bad callback pointer to midway into the callback function. This
  // should cause a CFG violation in MS code.
  auto bad_callback_func = reinterpret_cast<ProcessCallbackRoutineType>(
      (reinterpret_cast<uintptr_t>(CopyProgressRoutine)) + 0x1);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file_path = temp_dir.GetPath().AppendASCII("file.dat");
  EXPECT_EXIT(
      // CopyFileEx calls back into our code.
      CopyFileExW(exe_path.value().c_str(), temp_file_path.value().c_str(),
                  bad_callback_func, nullptr, FALSE, 0),
      ::testing::ExitedWithCode(STATUS_STACK_BUFFER_OVERRUN), "");
}

#endif  // !defined(_DEBUG) && !defined(ADDRESS_SANITIZER)

}  // namespace sandbox
