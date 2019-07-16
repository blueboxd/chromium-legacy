// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/dom_storage/local_storage_context_mojo.h"
#include "content/browser/dom_storage/session_storage_context_mojo.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"

namespace content {

class DOMStorageContextWrapperTest : public testing::Test {
 public:
  DOMStorageContextWrapperTest() = default;

  void SetUp() override {
    storage_policy_ = new MockSpecialStoragePolicy();
    fake_mojo_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    context_ = new DOMStorageContextWrapper(
        /*legacy_local_storage_path=*/base::FilePath(), fake_mojo_task_runner_,
        /*mojo_local_storage_context=*/nullptr,
        new SessionStorageContextMojo(
            fake_mojo_task_runner_, /*connector=*/nullptr,
            SessionStorageContextMojo::BackingMode::kNoDisk,
            /*local_partition_directory=*/base::FilePath(),
            /*leveldb_name=*/""));
  }

  void TearDown() override {
    context_->Shutdown();
    context_.reset();
    fake_mojo_task_runner_->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> fake_mojo_task_runner_;
  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  scoped_refptr<DOMStorageContextWrapper> context_;

  DISALLOW_COPY_AND_ASSIGN(DOMStorageContextWrapperTest);
};

TEST_F(DOMStorageContextWrapperTest, BadMessageScheduling) {
  blink::mojom::SessionStorageNamespacePtr ss_namespace_ptr;
  auto request = mojo::MakeRequest(&ss_namespace_ptr);
  bool called = false;
  // This call is invalid because |CreateSessionNamespace| was never called on
  // the SessionStorage context.
  context_->OpenSessionStorage(
      0, "nonexistant-namespace",
      base::BindLambdaForTesting(
          [&called](const std::string& message) { called = true; }),
      std::move(request));
  EXPECT_FALSE(called);
  fake_mojo_task_runner_->RunPendingTasks();

  // There should not be an error yet, as the callback has to run on
  // 'this' task runner and not the fake one.
  EXPECT_FALSE(called);

  // Cycle the current task runner.
  base::RunLoop loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   loop.QuitClosure());
  loop.Run();

  // The callback should have run now.
  EXPECT_TRUE(called);
}

}  // namespace content
