// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/file_manager.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace paint_preview {

TEST(FileManagerTest, TestStats) {
  base::Time now = base::Time::Now();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FileManager manager(temp_dir.GetPath());
  auto valid_key = manager.CreateKey(GURL("https://www.chromium.org"));
  auto missing_key = manager.CreateKey(GURL("https://www.muimorhc.org"));
  base::FilePath directory;
  EXPECT_TRUE(manager.CreateOrGetDirectory(valid_key, &directory));
  EXPECT_FALSE(directory.empty());
  EXPECT_TRUE(manager.DirectoryExists(valid_key));
  EXPECT_FALSE(manager.DirectoryExists(missing_key));

  base::Time created_time;
  EXPECT_FALSE(manager.GetCreatedTime(missing_key, &created_time));
  EXPECT_TRUE(manager.GetCreatedTime(valid_key, &created_time));

  base::TouchFile(directory, now - base::TimeDelta::FromSeconds(1),
                  now - base::TimeDelta::FromSeconds(1));
  base::Time accessed_time;
  EXPECT_TRUE(manager.GetLastModifiedTime(valid_key, &accessed_time));
  base::FilePath proto_path = directory.AppendASCII("paint_preview.pb");
  base::File file(proto_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  const size_t kSize = 50;
  std::string zeros(kSize, '0');
  file.Write(0, zeros.data(), zeros.size());
  file.Close();
  base::TouchFile(directory, now + base::TimeDelta::FromSeconds(1),
                  now + base::TimeDelta::FromSeconds(1));
  base::Time later_accessed_time;
  EXPECT_FALSE(manager.GetLastModifiedTime(missing_key, &created_time));
  EXPECT_TRUE(manager.GetLastModifiedTime(valid_key, &later_accessed_time));
  EXPECT_GT(later_accessed_time, accessed_time);

  EXPECT_GE(manager.GetSizeOfArtifacts(valid_key), kSize);
}

TEST(FileManagerTest, TestCreateOrGetDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FileManager manager(temp_dir.GetPath());
  auto key = manager.CreateKey(1U);

  // Create a new directory.
  base::FilePath new_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectory(key, &new_directory));
  EXPECT_FALSE(new_directory.empty());
  EXPECT_TRUE(base::PathExists(new_directory));

  // Open an existing directory.
  base::FilePath existing_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectory(key, &existing_directory));
  EXPECT_FALSE(existing_directory.empty());
  EXPECT_EQ(existing_directory, new_directory);
  EXPECT_TRUE(base::PathExists(existing_directory));

  // A file needs to exist for compression to work.
  base::FilePath test_file_path = existing_directory.AppendASCII("foo.txt");
  {
    base::File file(test_file_path,
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    const std::string data = "foobarbaz";
    file.WriteAtCurrentPos(data.data(), data.size());
  }
  EXPECT_TRUE(base::PathExists(test_file_path));
  base::FilePath test_file_path_empty =
      existing_directory.AppendASCII("bar.txt");
  {
    base::File file(test_file_path_empty,
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }
  EXPECT_TRUE(base::PathExists(test_file_path_empty));

  // Compress.
  base::FilePath zip_path = existing_directory.AddExtensionASCII(".zip");
  EXPECT_TRUE(manager.CompressDirectory(key));
  EXPECT_FALSE(base::PathExists(existing_directory));
  EXPECT_FALSE(base::PathExists(test_file_path));
  EXPECT_FALSE(base::PathExists(test_file_path_empty));
  EXPECT_TRUE(base::PathExists(zip_path));
  EXPECT_GT(manager.GetSizeOfArtifacts(key), 0U);
  existing_directory.clear();

  // Open a compressed file.
  EXPECT_TRUE(manager.CreateOrGetDirectory(key, &existing_directory));
  EXPECT_EQ(existing_directory, new_directory);
  EXPECT_TRUE(base::PathExists(existing_directory));
  EXPECT_TRUE(base::PathExists(test_file_path));
  EXPECT_TRUE(base::PathExists(test_file_path_empty));
  EXPECT_FALSE(base::PathExists(zip_path));
}

TEST(FileManagerTest, TestCompressDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FileManager manager(temp_dir.GetPath());
  auto key = manager.CreateKey(GURL("https://www.chromium.org"));

  base::FilePath new_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectory(key, &new_directory));
  EXPECT_FALSE(new_directory.empty());
  EXPECT_TRUE(base::PathExists(new_directory));

  // Compression fails without valid contents.
  base::FilePath zip_path = new_directory.AddExtensionASCII(".zip");
  EXPECT_FALSE(manager.CompressDirectory(key));
  EXPECT_TRUE(base::PathExists(new_directory));
  EXPECT_FALSE(base::PathExists(zip_path));

  // Create a file with contents for compression to work.
  {
    base::File file(new_directory.AppendASCII("foo.txt"),
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    const std::string data = "foobarbaz";
    file.WriteAtCurrentPos(data.data(), data.size());
  }

  EXPECT_TRUE(manager.CompressDirectory(key));
  EXPECT_FALSE(base::PathExists(new_directory));
  EXPECT_TRUE(base::PathExists(zip_path));
}

TEST(FileManagerTest, TestDeleteArtifactsFor) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FileManager manager(temp_dir.GetPath());

  auto cr_key = manager.CreateKey(GURL("https://www.chromium.org"));
  base::FilePath cr_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectory(cr_key, &cr_directory));
  EXPECT_FALSE(cr_directory.empty());
  EXPECT_TRUE(base::PathExists(cr_directory));

  auto w3_key = manager.CreateKey(GURL("https://www.w3.org"));
  base::FilePath w3_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectory(w3_key, &w3_directory));
  EXPECT_FALSE(w3_directory.empty());
  EXPECT_TRUE(base::PathExists(w3_directory));

  manager.DeleteArtifacts(cr_key);
  EXPECT_FALSE(base::PathExists(cr_directory));
  EXPECT_TRUE(base::PathExists(w3_directory));

  base::FilePath new_cr_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectory(cr_key, &new_cr_directory));
  EXPECT_FALSE(new_cr_directory.empty());
  EXPECT_TRUE(base::PathExists(new_cr_directory));
  EXPECT_EQ(cr_directory, new_cr_directory);

  manager.DeleteArtifacts(std::vector<DirectoryKey>({cr_key, w3_key}));
  EXPECT_FALSE(base::PathExists(new_cr_directory));
  EXPECT_FALSE(base::PathExists(w3_directory));
}

TEST(FileManagerTest, TestDeleteAll) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FileManager manager(temp_dir.GetPath());
  auto cr_key = manager.CreateKey(GURL("https://www.chromium.org"));
  base::FilePath cr_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectory(cr_key, &cr_directory));
  EXPECT_FALSE(cr_directory.empty());
  EXPECT_TRUE(base::PathExists(cr_directory));
  auto w3_key = manager.CreateKey(GURL("https://www.w3.org"));
  base::FilePath w3_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectory(w3_key, &w3_directory));
  EXPECT_FALSE(w3_directory.empty());
  EXPECT_TRUE(base::PathExists(w3_directory));
  manager.DeleteAll();
  EXPECT_FALSE(base::PathExists(cr_directory));
  EXPECT_FALSE(base::PathExists(w3_directory));
}

TEST(FileManagerTest, HandleProto) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FileManager manager(temp_dir.GetPath());
  auto key = manager.CreateKey(1U);
  base::FilePath path;
  EXPECT_TRUE(manager.CreateOrGetDirectory(key, &path));

  PaintPreviewProto original_proto;
  auto* root_frame = original_proto.mutable_root_frame();
  root_frame->set_embedding_token_low(0);
  root_frame->set_embedding_token_high(0);
  root_frame->set_is_main_frame(true);
  root_frame->set_file_path(path.AppendASCII("0.skp").MaybeAsASCII());
  auto* metadata = original_proto.mutable_metadata();
  metadata->set_url(GURL("www.chromium.org").spec());

  EXPECT_TRUE(manager.SerializePaintPreviewProto(key, original_proto));
  EXPECT_TRUE(base::PathExists(path.AppendASCII("proto.pb")));
  auto read_proto = manager.DeserializePaintPreviewProto(key);
  EXPECT_NE(read_proto, nullptr);
  EXPECT_THAT(*read_proto, EqualsProto(original_proto));
}

}  // namespace paint_preview
