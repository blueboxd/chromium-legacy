// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "media/mojo/interfaces/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using media::mojom::CdmFile;
using media::mojom::CdmFileAssociatedPtr;
using media::mojom::CdmFileAssociatedPtrInfo;
using media::mojom::CdmStorage;
using media::mojom::CdmStoragePtr;

namespace content {

namespace {

const char kTestFileSystemId[] = "test_file_system";
const char kTestOrigin[] = "http://www.test.com";

// Helper functions to manipulate RenderFrameHosts.

void SimulateNavigation(RenderFrameHost** rfh, const GURL& url) {
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(url, *rfh);
  navigation_simulator->Commit();
  *rfh = navigation_simulator->GetFinalRenderFrameHost();
}

}  // namespace

class CdmStorageTest : public RenderViewHostTestHarness {
 public:
  CdmStorageTest()
      : RenderViewHostTestHarness(
            content::TestBrowserThreadBundle::REAL_IO_THREAD) {}

 protected:
  void SetUp() final {
    RenderViewHostTestHarness::SetUp();
    rfh_ = web_contents()->GetMainFrame();
    RenderFrameHostTester::For(rfh_)->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&rfh_, GURL(kTestOrigin));
  }

  // Creates and initializes the CdmStorage object using |file_system_id|.
  // Returns true if successful, false otherwise.
  void Initialize(const std::string& file_system_id) {
    DVLOG(3) << __func__;

    // Create the CdmStorageImpl object. |cdm_storage_| will own the resulting
    // object.
    CdmStorageImpl::Create(rfh_, file_system_id,
                           mojo::MakeRequest(&cdm_storage_));
  }

  // Open the file |name|. Returns true if the file returned is valid, false
  // otherwise. On success |cdm_file| is bound to the CdmFileImpl object.
  bool Open(const std::string& name,
            CdmFileAssociatedPtr* cdm_file) {
    DVLOG(3) << __func__;

    CdmStorage::Status status;
    cdm_storage_->Open(
        name, base::BindOnce(&CdmStorageTest::OpenDone, base::Unretained(this),
                             &status, cdm_file));
    RunAndWaitForResult();
    return status == CdmStorage::Status::kSuccess;
  }

  // Reads the contents of the previously opened |cdm_file|. If successful,
  // true is returned and |data| is updated with the contents of the file.
  // If unable to read the file, false is returned.
  bool Read(CdmFile* cdm_file, std::vector<uint8_t>* data) {
    DVLOG(3) << __func__;

    CdmFile::Status status;
    cdm_file->Read(base::BindOnce(&CdmStorageTest::FileRead,
                                  base::Unretained(this), &status, data));
    RunAndWaitForResult();
    return status == CdmFile::Status::kSuccess;
  }

  // Writes |data| to the previously opened |cdm_file|, replacing the contents
  // of the file. Returns true if successful, false otherwise.
  bool Write(CdmFile* cdm_file, const std::vector<uint8_t>& data) {
    DVLOG(3) << __func__;

    CdmFile::Status status;
    cdm_file->Write(data, base::BindOnce(&CdmStorageTest::FileWritten,
                                         base::Unretained(this), &status));
    RunAndWaitForResult();
    return status == CdmFile::Status::kSuccess;
  }

 private:
  void OpenDone(CdmStorage::Status* status,
                CdmFileAssociatedPtr* cdm_file,
                CdmStorage::Status actual_status,
                CdmFileAssociatedPtrInfo actual_cdm_file) {
    DVLOG(3) << __func__;
    *status = actual_status;

    // Open() returns a CdmFileAssociatedPtrInfo, so bind it to the
    // CdmFileAssociatedPtr provided.
    CdmFileAssociatedPtr cdm_file_ptr;
    cdm_file_ptr.Bind(std::move(actual_cdm_file));
    *cdm_file = std::move(cdm_file_ptr);
    run_loop_->Quit();
  }

  void FileRead(CdmFile::Status* status,
                std::vector<uint8_t>* data,
                CdmFile::Status actual_status,
                const std::vector<uint8_t>& actual_data) {
    DVLOG(3) << __func__;
    *status = actual_status;
    *data = actual_data;
    run_loop_->Quit();
  }

  void FileWritten(CdmFile::Status* status, CdmFile::Status actual_status) {
    DVLOG(3) << __func__;
    *status = actual_status;
    run_loop_->Quit();
  }

  // Start running and allow the asynchronous IO operations to complete.
  void RunAndWaitForResult() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  RenderFrameHost* rfh_ = nullptr;
  CdmStoragePtr cdm_storage_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

TEST_F(CdmStorageTest, InvalidFileSystemIdWithSlash) {
  Initialize("name/");

  const char kFileName[] = "valid_file_name";
  CdmFileAssociatedPtr cdm_file;
  EXPECT_FALSE(Open(kFileName, &cdm_file));
  EXPECT_FALSE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, InvalidFileSystemIdWithBackSlash) {
  Initialize("name\\");

  const char kFileName[] = "valid_file_name";
  CdmFileAssociatedPtr cdm_file;
  EXPECT_FALSE(Open(kFileName, &cdm_file));
  EXPECT_FALSE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, InvalidFileSystemIdEmpty) {
  Initialize("");

  const char kFileName[] = "valid_file_name";
  CdmFileAssociatedPtr cdm_file;
  EXPECT_FALSE(Open(kFileName, &cdm_file));
  EXPECT_FALSE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, InvalidFileName) {
  Initialize(kTestFileSystemId);

  // Anything other than ASCII letter, digits, and -._ will fail. Add a
  // Unicode character to the name.
  const char kFileName[] = "openfile\u1234";
  CdmFileAssociatedPtr cdm_file;
  EXPECT_FALSE(Open(kFileName, &cdm_file));
  EXPECT_FALSE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, InvalidFileNameEmpty) {
  Initialize(kTestFileSystemId);

  const char kFileName[] = "";
  CdmFileAssociatedPtr cdm_file;
  EXPECT_FALSE(Open(kFileName, &cdm_file));
  EXPECT_FALSE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, InvalidFileNameStartWithUnderscore) {
  Initialize(kTestFileSystemId);

  const char kFileName[] = "_invalid";
  CdmFileAssociatedPtr cdm_file;
  EXPECT_FALSE(Open(kFileName, &cdm_file));
  EXPECT_FALSE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, InvalidFileNameTooLong) {
  Initialize(kTestFileSystemId);

  // Limit is 256 characters, so try a file name with 257.
  const std::string kFileName(257, 'a');
  CdmFileAssociatedPtr cdm_file;
  EXPECT_FALSE(Open(kFileName, &cdm_file));
  EXPECT_FALSE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, OpenFile) {
  Initialize(kTestFileSystemId);

  const char kFileName[] = "test_file_name";
  CdmFileAssociatedPtr cdm_file;
  EXPECT_TRUE(Open(kFileName, &cdm_file));
  EXPECT_TRUE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, OpenFileLocked) {
  Initialize(kTestFileSystemId);

  const char kFileName[] = "test_file_name";
  CdmFileAssociatedPtr cdm_file1;
  EXPECT_TRUE(Open(kFileName, &cdm_file1));
  EXPECT_TRUE(cdm_file1.is_bound());

  // Second attempt on the same file should fail as the file is locked.
  CdmFileAssociatedPtr cdm_file2;
  EXPECT_FALSE(Open(kFileName, &cdm_file2));
  EXPECT_FALSE(cdm_file2.is_bound());

  // Now close the first file and try again. It should be free now.
  cdm_file1.reset();

  CdmFileAssociatedPtr cdm_file3;
  EXPECT_TRUE(Open(kFileName, &cdm_file3));
  EXPECT_TRUE(cdm_file3.is_bound());
}

TEST_F(CdmStorageTest, MultipleFiles) {
  Initialize(kTestFileSystemId);

  const char kFileName1[] = "file1";
  CdmFileAssociatedPtr cdm_file1;
  EXPECT_TRUE(Open(kFileName1, &cdm_file1));
  EXPECT_TRUE(cdm_file1.is_bound());

  const char kFileName2[] = "file2";
  CdmFileAssociatedPtr cdm_file2;
  EXPECT_TRUE(Open(kFileName2, &cdm_file2));
  EXPECT_TRUE(cdm_file2.is_bound());

  const char kFileName3[] = "file3";
  CdmFileAssociatedPtr cdm_file3;
  EXPECT_TRUE(Open(kFileName3, &cdm_file3));
  EXPECT_TRUE(cdm_file3.is_bound());
}

TEST_F(CdmStorageTest, WriteThenReadFile) {
  Initialize(kTestFileSystemId);

  const char kFileName[] = "test_file_name";
  CdmFileAssociatedPtr cdm_file;
  EXPECT_TRUE(Open(kFileName, &cdm_file));
  EXPECT_TRUE(cdm_file.is_bound());

  // Write several bytes and read them back.
  std::vector<uint8_t> kTestData = {'r', 'a', 'n', 'd', 'o', 'm'};
  EXPECT_TRUE(Write(cdm_file.get(), kTestData));

  std::vector<uint8_t> data_read;
  EXPECT_TRUE(Read(cdm_file.get(), &data_read));
  EXPECT_EQ(kTestData, data_read);
}

TEST_F(CdmStorageTest, ReadThenWriteEmptyFile) {
  Initialize(kTestFileSystemId);

  const char kFileName[] = "empty_file_name";
  CdmFileAssociatedPtr cdm_file;
  EXPECT_TRUE(Open(kFileName, &cdm_file));
  EXPECT_TRUE(cdm_file.is_bound());

  // New file should be empty.
  std::vector<uint8_t> data_read;
  EXPECT_TRUE(Read(cdm_file.get(), &data_read));
  EXPECT_EQ(0u, data_read.size());

  // Write nothing.
  EXPECT_TRUE(Write(cdm_file.get(), std::vector<uint8_t>()));

  // Should still be empty.
  EXPECT_TRUE(Read(cdm_file.get(), &data_read));
  EXPECT_EQ(0u, data_read.size());
}

}  // namespace content
