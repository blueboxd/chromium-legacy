// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fileapi/file.h"

#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/file/file_utilities.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

class MockFileUtilitiesHost : public mojom::blink::FileUtilitiesHost {
 public:
  MockFileUtilitiesHost()
      : broker_(*Platform::Current()->GetBrowserInterfaceBroker()) {
    broker_.SetBinderForTesting(
        FileUtilitiesHost::Name_,
        WTF::BindRepeating(&MockFileUtilitiesHost::BindReceiver,
                           WTF::Unretained(this)));
    RebindFileUtilitiesForTesting();
  }

  ~MockFileUtilitiesHost() override {
    broker_.SetBinderForTesting(FileUtilitiesHost::Name_, {});
    RebindFileUtilitiesForTesting();
  }

  void SetFileInfoToBeReturned(const base::File::Info info) {
    file_info_ = info;
  }

 private:
  void BindReceiver(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(this,
                   mojo::PendingReceiver<FileUtilitiesHost>(std::move(handle)));
  }

  // FileUtilitiesHost function:
  void GetFileInfo(const base::FilePath& path,
                   GetFileInfoCallback callback) override {
    std::move(callback).Run(file_info_);
  }

  ThreadSafeBrowserInterfaceBrokerProxy& broker_;
  mojo::ReceiverSet<FileUtilitiesHost> receivers_;
  base::File::Info file_info_;
};

void ExpectTimestampIsNow(const File& file) {
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta_now = now - base::Time::UnixEpoch();
  // Because lastModified() applies floor() internally, we should compare
  // integral millisecond values.
  EXPECT_GE(file.lastModified(), delta_now.InMilliseconds());
  EXPECT_GE(file.LastModifiedTime(), now);
}

}  // namespace

TEST(FileTest, NativeFileWithoutTimestamp) {
  MockFileUtilitiesHost host;
  base::File::Info info;
  info.last_modified = base::Time();
  host.SetFileInfoToBeReturned(info);

  auto* const file = MakeGarbageCollected<File>("/native/path");
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/path", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  ExpectTimestampIsNow(*file);
}

TEST(FileTest, NativeFileWithUnixEpochTimestamp) {
  MockFileUtilitiesHost host;
  base::File::Info info;
  info.last_modified = base::Time::UnixEpoch();
  host.SetFileInfoToBeReturned(info);

  auto* const file = MakeGarbageCollected<File>("/native/path");
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ(0, file->lastModified());
  EXPECT_EQ(base::Time::UnixEpoch(), file->LastModifiedTime());
}

TEST(FileTest, NativeFileWithApocalypseTimestamp) {
  MockFileUtilitiesHost host;
  base::File::Info info;
  info.last_modified = base::Time::Max();
  host.SetFileInfoToBeReturned(info);

  auto* const file = MakeGarbageCollected<File>("/native/path");
  EXPECT_TRUE(file->HasBackingFile());

  EXPECT_EQ((base::Time::Max() - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(base::Time::Max(), file->LastModifiedTime());
}

TEST(FileTest, BlobBackingFileWithoutTimestamp) {
  auto* const file = MakeGarbageCollected<File>("name", base::nullopt,
                                                BlobDataHandle::Create());
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().IsEmpty());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  ExpectTimestampIsNow(*file);
}

TEST(FileTest, BlobBackingFileWithWindowsEpochTimestamp) {
  auto* const file = MakeGarbageCollected<File>("name", base::Time(),
                                                BlobDataHandle::Create());
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().IsEmpty());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ((base::Time() - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(base::Time(), file->LastModifiedTime());
}

TEST(FileTest, BlobBackingFileWithUnixEpochTimestamp) {
  const scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create();
  auto* const file = MakeGarbageCollected<File>("name", base::Time::UnixEpoch(),
                                                blob_data_handle);
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().IsEmpty());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ(INT64_C(0), file->lastModified());
  EXPECT_EQ(base::Time::UnixEpoch(), file->LastModifiedTime());
}

TEST(FileTest, BlobBackingFileWithApocalypseTimestamp) {
  constexpr base::Time kMaxTime = base::Time::Max();
  auto* const file =
      MakeGarbageCollected<File>("name", kMaxTime, BlobDataHandle::Create());
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().IsEmpty());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ((kMaxTime - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(kMaxTime, file->LastModifiedTime());
}

TEST(FileTest, fileSystemFileWithNativeSnapshot) {
  FileMetadata metadata;
  metadata.platform_path = "/native/snapshot";
  File* const file =
      File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
}

TEST(FileTest, fileSystemFileWithNativeSnapshotAndSize) {
  FileMetadata metadata;
  metadata.length = 1024ll;
  metadata.platform_path = "/native/snapshot";
  File* const file =
      File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
}

TEST(FileTest, FileSystemFileWithWindowsEpochTimestamp) {
  FileMetadata metadata;
  metadata.length = INT64_C(1025);
  metadata.modification_time = base::Time();
  metadata.platform_path = "/native/snapshot";
  File* const file =
      File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ(UINT64_C(1025), file->size());
  EXPECT_EQ((base::Time() - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(base::Time(), file->LastModifiedTime());
}

TEST(FileTest, FileSystemFileWithUnixEpochTimestamp) {
  FileMetadata metadata;
  metadata.length = INT64_C(1025);
  metadata.modification_time = base::Time::UnixEpoch();
  metadata.platform_path = "/native/snapshot";
  File* const file =
      File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ(UINT64_C(1025), file->size());
  EXPECT_EQ(INT64_C(0), file->lastModified());
  EXPECT_EQ(base::Time::UnixEpoch(), file->LastModifiedTime());
}

TEST(FileTest, FileSystemFileWithApocalypseTimestamp) {
  constexpr base::Time kMaxTime = base::Time::Max();
  FileMetadata metadata;
  metadata.length = INT64_C(1025);
  metadata.modification_time = kMaxTime;
  metadata.platform_path = "/native/snapshot";
  File* const file =
      File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ(UINT64_C(1025), file->size());
  EXPECT_EQ((kMaxTime - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(kMaxTime, file->LastModifiedTime());
}

TEST(FileTest, fileSystemFileWithoutNativeSnapshot) {
  KURL url("filesystem:http://example.com/isolated/hash/non-native-file");
  FileMetadata metadata;
  File* const file =
      File::CreateForFileSystemFile(url, metadata, File::kIsUserVisible);
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().IsEmpty());
  EXPECT_EQ(url, file->FileSystemURL());
}

TEST(FileTest, hsaSameSource) {
  auto* const native_file_a1 = MakeGarbageCollected<File>("/native/pathA");
  auto* const native_file_a2 = MakeGarbageCollected<File>("/native/pathA");
  auto* const native_file_b = MakeGarbageCollected<File>("/native/pathB");

  const scoped_refptr<BlobDataHandle> blob_data_a = BlobDataHandle::Create();
  const scoped_refptr<BlobDataHandle> blob_data_b = BlobDataHandle::Create();
  const base::Time kEpoch = base::Time::UnixEpoch();
  auto* const blob_file_a1 =
      MakeGarbageCollected<File>("name", kEpoch, blob_data_a);
  auto* const blob_file_a2 =
      MakeGarbageCollected<File>("name", kEpoch, blob_data_a);
  auto* const blob_file_b =
      MakeGarbageCollected<File>("name", kEpoch, blob_data_b);

  KURL url_a("filesystem:http://example.com/isolated/hash/non-native-file-A");
  KURL url_b("filesystem:http://example.com/isolated/hash/non-native-file-B");
  FileMetadata metadata;
  File* const file_system_file_a1 =
      File::CreateForFileSystemFile(url_a, metadata, File::kIsUserVisible);
  File* const file_system_file_a2 =
      File::CreateForFileSystemFile(url_a, metadata, File::kIsUserVisible);
  File* const file_system_file_b =
      File::CreateForFileSystemFile(url_b, metadata, File::kIsUserVisible);

  EXPECT_FALSE(native_file_a1->HasSameSource(*blob_file_a1));
  EXPECT_FALSE(blob_file_a1->HasSameSource(*file_system_file_a1));
  EXPECT_FALSE(file_system_file_a1->HasSameSource(*native_file_a1));

  EXPECT_TRUE(native_file_a1->HasSameSource(*native_file_a1));
  EXPECT_TRUE(native_file_a1->HasSameSource(*native_file_a2));
  EXPECT_FALSE(native_file_a1->HasSameSource(*native_file_b));

  EXPECT_TRUE(blob_file_a1->HasSameSource(*blob_file_a1));
  EXPECT_TRUE(blob_file_a1->HasSameSource(*blob_file_a2));
  EXPECT_FALSE(blob_file_a1->HasSameSource(*blob_file_b));

  EXPECT_TRUE(file_system_file_a1->HasSameSource(*file_system_file_a1));
  EXPECT_TRUE(file_system_file_a1->HasSameSource(*file_system_file_a2));
  EXPECT_FALSE(file_system_file_a1->HasSameSource(*file_system_file_b));
}

}  // namespace blink
