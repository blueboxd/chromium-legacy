// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/pe_resource.h"

#include <windows.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/installer/mini_installer/mini_file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mini_installer {

TEST(PEResourceTest, WriteToDisk) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath manifest_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("manifest"));

  PEResource manifest(MAKEINTRESOURCE(1), RT_MANIFEST,
                      ::GetModuleHandle(nullptr));
  ASSERT_TRUE(manifest.IsValid());

  MiniFile manifest_file(MiniFile::DeleteOnClose::kYes);
  ASSERT_TRUE(
      manifest.WriteToDisk(manifest_path.value().c_str(), manifest_file));

  ASSERT_TRUE(manifest_file.IsValid());
  EXPECT_PRED1(base::PathExists, manifest_path);

  manifest_file.Close();
  EXPECT_FALSE(base::PathExists(manifest_path));

  EXPECT_TRUE(temp_dir.Delete());
}

}  // namespace mini_installer
