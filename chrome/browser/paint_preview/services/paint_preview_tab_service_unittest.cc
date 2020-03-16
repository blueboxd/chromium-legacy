// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/paint_preview/services/paint_preview_tab_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace paint_preview {

namespace {

constexpr char kFeatureName[] = "tab_service_test";

class MockPaintPreviewRecorder : public mojom::PaintPreviewRecorder {
 public:
  MockPaintPreviewRecorder() = default;
  ~MockPaintPreviewRecorder() override = default;

  MockPaintPreviewRecorder(const MockPaintPreviewRecorder&) = delete;
  MockPaintPreviewRecorder& operator=(const MockPaintPreviewRecorder&) = delete;

  void CapturePaintPreview(
      mojom::PaintPreviewCaptureParamsPtr params,
      mojom::PaintPreviewRecorder::CapturePaintPreviewCallback callback)
      override {
    std::move(callback).Run(status_, mojom::PaintPreviewCaptureResponse::New());
  }

  void SetResponse(mojom::PaintPreviewStatus status) { status_ = status; }

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.Bind(mojo::PendingAssociatedReceiver<mojom::PaintPreviewRecorder>(
        std::move(handle)));
  }

 private:
  mojom::PaintPreviewStatus status_;
  mojo::AssociatedReceiver<mojom::PaintPreviewRecorder> binding_{this};
};

std::vector<base::FilePath> ListDir(const base::FilePath& path) {
  base::FileEnumerator enumerator(
      path, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
      FILE_PATH_LITERAL("*.skp"));  // Ignore the proto.pb files.
  std::vector<base::FilePath> files;
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    files.push_back(name);
  }
  return files;
}

bool CaptureExists(PaintPreviewTabService* service, int tab_id) {
  bool out = false;
  base::RunLoop loop;
  service->HasCaptureForTab(
      tab_id, base::BindOnce(
                  [](base::OnceClosure quit, bool* out, bool success) {
                    *out = success;
                    std::move(quit).Run();
                  },
                  loop.QuitClosure(), &out));
  loop.Run();
  return out;
}

}  // namespace

class PaintPreviewTabServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  PaintPreviewTabServiceTest() = default;
  ~PaintPreviewTabServiceTest() override = default;

  PaintPreviewTabServiceTest(const PaintPreviewTabServiceTest&) = delete;
  PaintPreviewTabServiceTest& operator=(const PaintPreviewTabServiceTest&) =
      delete;

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    service_ = std::make_unique<PaintPreviewTabService>(
        temp_dir_.GetPath(), kFeatureName, nullptr, false);
  }

  PaintPreviewTabService* GetService() { return service_.get(); }

  void OverrideInterface(MockPaintPreviewRecorder* recorder) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::PaintPreviewRecorder::Name_,
        base::BindRepeating(&MockPaintPreviewRecorder::BindRequest,
                            base::Unretained(recorder)));
  }

 private:
  std::unique_ptr<PaintPreviewTabService> service_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(PaintPreviewTabServiceTest, CaptureTab) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.example.com"));
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(mojom::PaintPreviewStatus::kOk);
  OverrideInterface(&recorder);

  auto* service = GetService();
  base::RunLoop loop;
  service->CaptureTab(
      kTabId, web_contents(),
      base::BindOnce(
          [](base::OnceClosure quit, PaintPreviewTabService::Status status) {
            EXPECT_EQ(status, PaintPreviewTabService::Status::kOk);
            std::move(quit).Run();
          },
          loop.QuitClosure()));
  loop.Run();

  EXPECT_TRUE(CaptureExists(service, kTabId));

  auto file_manager = service->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  content::RunAllTasksUntilIdle();

  service->TabClosed(kTabId);
  EXPECT_FALSE(CaptureExists(service, kTabId));
  content::RunAllTasksUntilIdle();
  service->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  content::RunAllTasksUntilIdle();
}

TEST_F(PaintPreviewTabServiceTest, CaptureTabFailed) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.example.com"));
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(mojom::PaintPreviewStatus::kFailed);
  OverrideInterface(&recorder);

  auto* service = GetService();
  base::RunLoop loop;
  service->CaptureTab(
      kTabId, web_contents(),
      base::BindOnce(
          [](base::OnceClosure quit, PaintPreviewTabService::Status status) {
            EXPECT_EQ(status, PaintPreviewTabService::Status::kCaptureFailed);
            std::move(quit).Run();
          },
          loop.QuitClosure()));
  loop.Run();

  auto file_manager = service->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  content::RunAllTasksUntilIdle();

  service->TabClosed(kTabId);
  content::RunAllTasksUntilIdle();
  service->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  content::RunAllTasksUntilIdle();
}

TEST_F(PaintPreviewTabServiceTest, CaptureTabTwice) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.example.com"));
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(mojom::PaintPreviewStatus::kOk);
  OverrideInterface(&recorder);

  auto* service = GetService();
  base::RunLoop loop_1;
  service->CaptureTab(
      kTabId, web_contents(),
      base::BindOnce(
          [](base::OnceClosure quit, PaintPreviewTabService::Status status) {
            EXPECT_EQ(status, PaintPreviewTabService::Status::kOk);
            std::move(quit).Run();
          },
          loop_1.QuitClosure()));
  loop_1.Run();
  auto file_manager = service->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  content::RunAllTasksUntilIdle();
  content::RunAllTasksUntilIdle();
  base::FilePath path_1;
  service->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, file_manager, key,
                     false),
      base::BindOnce(
          [](base::FilePath* out, const base::Optional<base::FilePath>& path) {
            EXPECT_TRUE(path.has_value());
            *out = path.value();
          },
          &path_1));
  content::RunAllTasksUntilIdle();
  auto files_1 = ListDir(path_1);
  ASSERT_EQ(1U, files_1.size());

  base::RunLoop loop_2;
  service->CaptureTab(
      kTabId, web_contents(),
      base::BindOnce(
          [](base::OnceClosure quit, PaintPreviewTabService::Status status) {
            EXPECT_EQ(status, PaintPreviewTabService::Status::kOk);
            std::move(quit).Run();
          },
          loop_2.QuitClosure()));
  loop_2.Run();

  service->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  base::FilePath path_2;
  service->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, file_manager, key,
                     false),
      base::BindOnce(
          [](base::FilePath* out, const base::Optional<base::FilePath>& path) {
            EXPECT_TRUE(path.has_value());
            *out = path.value();
          },
          &path_2));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(path_2, path_1);
  auto files_2 = ListDir(path_2);
  ASSERT_EQ(1U, files_2.size());
  EXPECT_NE(files_1, files_2);

  service->TabClosed(kTabId);
  service->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  content::RunAllTasksUntilIdle();
}

TEST_F(PaintPreviewTabServiceTest, TestUnityAudit) {
  auto* service = GetService();
  auto file_manager = service->GetFileManager();

  std::vector<int> tab_ids = {1, 2, 3};

  base::FilePath path;
  for (const auto& id : tab_ids) {
    auto key = file_manager->CreateKey(id);
    service->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](scoped_refptr<FileManager> file_manager,
                          const DirectoryKey& key) {
                         file_manager->CreateOrGetDirectory(key, false);
                         EXPECT_TRUE(file_manager->DirectoryExists(key));
                       },
                       file_manager, key));
  }
  content::RunAllTasksUntilIdle();

  service->AuditArtifacts(tab_ids);
  content::RunAllTasksUntilIdle();

  for (const auto& id : tab_ids) {
    auto key = file_manager->CreateKey(id);
    service->GetTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
        base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  }
  content::RunAllTasksUntilIdle();
}

TEST_F(PaintPreviewTabServiceTest, TestDisjointAudit) {
  auto* service = GetService();

  auto file_manager = service->GetFileManager();

  std::vector<int> tab_ids = {1, 2, 3};

  base::FilePath path;
  for (const auto& id : tab_ids) {
    auto key = file_manager->CreateKey(id);
    service->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](scoped_refptr<FileManager> file_manager,
                          const DirectoryKey& key) {
                         file_manager->CreateOrGetDirectory(key, false);
                         EXPECT_TRUE(file_manager->DirectoryExists(key));
                       },
                       file_manager, key));
  }

  service->AuditArtifacts({4});
  content::RunAllTasksUntilIdle();

  for (const auto& id : tab_ids) {
    auto key = file_manager->CreateKey(id);
    service->GetTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
        base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  }
  content::RunAllTasksUntilIdle();
}

TEST_F(PaintPreviewTabServiceTest, TestPartialAudit) {
  auto* service = GetService();

  auto file_manager = service->GetFileManager();

  base::FilePath path;
  for (const auto& id : {1, 2, 3}) {
    auto key = file_manager->CreateKey(id);
    service->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](scoped_refptr<FileManager> file_manager,
                          const DirectoryKey& key) {
                         file_manager->CreateOrGetDirectory(key, false);
                         EXPECT_TRUE(file_manager->DirectoryExists(key));
                       },
                       file_manager, key));
  }

  std::vector<int> kept_tab_ids = {1, 3};
  service->AuditArtifacts(kept_tab_ids);
  content::RunAllTasksUntilIdle();

  for (const auto& id : kept_tab_ids) {
    auto key = file_manager->CreateKey(id);
    service->GetTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
        base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  }
  auto key = file_manager->CreateKey(2);
  service->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  content::RunAllTasksUntilIdle();
}

}  // namespace paint_preview
