// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/parallel_unpacker.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/pending_extension_info.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/verifier_formats.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"

namespace extensions {

class ParallelUnpackerTest : public testing::Test,
                             public ParallelUnpacker::Delegate {
 public:
  ParallelUnpackerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    ASSERT_TRUE(extensions_dir_.CreateUniqueTempDir());
    in_process_utility_thread_helper_.reset(
        new content::InProcessUtilityThreadHelper);
    parallel_unpacker_ = std::make_unique<ParallelUnpacker>(this, &profile_);
  }

  void TearDown() override {
    in_process_utility_thread_helper_.reset();
    parallel_unpacker_.reset();
  }

  base::FilePath GetCrxFullPath(const std::string& crx_name) {
    base::FilePath full_path;
    EXPECT_TRUE(base::PathService::Get(extensions::DIR_TEST_DATA, &full_path));
    full_path = full_path.AppendASCII("unpacker").AppendASCII(crx_name);
    EXPECT_TRUE(base::PathExists(full_path)) << full_path.value();
    return full_path;
  }

  void Unpack(const std::string& crx_name) {
    base::FilePath crx_path = GetCrxFullPath(crx_name);
    extensions::CRXFileInfo crx_info(crx_path, GetTestVerifierFormat());
    extensions::FetchedCRXFile fetch_info(crx_info, false, std::set<int>(),
                                          base::BindOnce([](bool) {}));
    extensions::PendingExtensionInfo pending_extension_info(
        "", "", GURL(), base::Version(), [](const Extension*) { return true; },
        false, Manifest::INTERNAL, Extension::NO_FLAGS, true, false);

    parallel_unpacker_->Unpack(std::move(fetch_info), &pending_extension_info,
                               nullptr, extensions_dir_.GetPath());
    in_progress_count_++;
  }

  // ParallelUnpacker::Delegate:
  void OnParallelUnpackSuccess(
      ParallelUnpacker::UnpackedExtension unpacked_extension) override {
    std::string file_name =
        unpacked_extension.fetch_info.info.path.BaseName().MaybeAsASCII();
    successful_unpacks_.emplace(file_name, std::move(unpacked_extension));
    if (--in_progress_count_ == 0)
      std::move(quit_closure_).Run();
  }
  void OnParallelUnpackFailure(FetchedCRXFile fetch_info,
                               CrxInstallError error) override {
    std::string file_name = fetch_info.info.path.BaseName().MaybeAsASCII();
    failed_unpacks_.emplace(file_name, std::move(error));
    if (--in_progress_count_ == 0)
      std::move(quit_closure_).Run();
  }

  void WaitForAllComplete() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 protected:
  std::map<std::string, ParallelUnpacker::UnpackedExtension>
      successful_unpacks_;
  std::map<std::string, CrxInstallError> failed_unpacks_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::ScopedTempDir extensions_dir_;

  std::unique_ptr<content::InProcessUtilityThreadHelper>
      in_process_utility_thread_helper_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  std::unique_ptr<ParallelUnpacker> parallel_unpacker_;
  base::OnceClosure quit_closure_;
  int in_progress_count_ = 0;
};

TEST_F(ParallelUnpackerTest, OneGood) {
  Unpack("good_package.crx");
  WaitForAllComplete();
  EXPECT_EQ(successful_unpacks_.size(), 1u);
  EXPECT_EQ(failed_unpacks_.size(), 0u);
}

TEST_F(ParallelUnpackerTest, TwoGoodInParallel) {
  Unpack("good_package.crx");
  Unpack("good_l10n.crx");
  WaitForAllComplete();
  EXPECT_EQ(successful_unpacks_.size(), 2u);
  EXPECT_EQ(failed_unpacks_.size(), 0u);
}

TEST_F(ParallelUnpackerTest, OneGoodAndOneBadInParallel) {
  Unpack("good_package.crx");
  Unpack("missing_default_data.crx");
  WaitForAllComplete();
  EXPECT_EQ(successful_unpacks_.size(), 1u);
  EXPECT_EQ(failed_unpacks_.size(), 1u);
  EXPECT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
            failed_unpacks_.find("missing_default_data.crx")->second.type());
}

}  // namespace extensions
