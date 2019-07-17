// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FILESYSTEM_FILES_TEST_BASE_H_
#define COMPONENTS_SERVICES_FILESYSTEM_FILES_TEST_BASE_H_

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "components/services/filesystem/public/mojom/file_system.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/test/test_service.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace filesystem {

class FilesTestBase : public testing::Test {
 public:
  FilesTestBase();
  ~FilesTestBase() override;

  // testing::Test:
  void SetUp() override;

 protected:
  // Note: This has an out parameter rather than returning the Directory remote,
  // since |ASSERT_...()| doesn't work with return values.
  void GetTemporaryRoot(mojo::Remote<mojom::Directory>* directory);

  service_manager::Connector* connector() { return test_service_.connector(); }
  mojo::Remote<mojom::FileSystem>& files() { return files_; }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestServiceManager test_service_manager_;
  service_manager::TestService test_service_;
  mojo::Remote<mojom::FileSystem> files_;

  DISALLOW_COPY_AND_ASSIGN(FilesTestBase);
};

}  // namespace filesystem

#endif  // COMPONENTS_SERVICES_FILESYSTEM_FILES_TEST_BASE_H_
