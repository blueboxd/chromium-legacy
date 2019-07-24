// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/service/service_manager_connection_impl.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {
namespace {

constexpr char kTestServiceName[] = "test service";

}  // namespace

using ServiceManagerConnectionImplTest = PlatformTest;

TEST_F(ServiceManagerConnectionImplTest, ServiceLaunch) {
  TestWebThreadBundle thread_bundle(
      TestWebThreadBundle::Options::REAL_IO_THREAD);
  service_manager::mojom::ServicePtr service;
  ServiceManagerConnectionImpl connection_impl(
      mojo::MakeRequest(&service),
      base::CreateSingleThreadTaskRunnerWithTraits({WebThread::IO}));
  ServiceManagerConnection& connection = connection_impl;
  base::RunLoop run_loop;
  connection.SetDefaultServiceRequestHandler(base::BindLambdaForTesting(
      [&run_loop](const std::string& service_name,
                  service_manager::mojom::ServiceRequest request) {
        EXPECT_EQ(kTestServiceName, service_name);
        run_loop.Quit();
      }));
  connection.Start();

  mojo::PendingRemote<service_manager::mojom::Service> packaged_service;
  mojo::PendingRemote<service_manager::mojom::ProcessMetadata> metadata;
  ignore_result(metadata.InitWithNewPipeAndPassReceiver());
  service->CreatePackagedServiceInstance(
      service_manager::Identity(kTestServiceName, base::Token::CreateRandom(),
                                base::Token(), base::Token::CreateRandom()),
      packaged_service.InitWithNewPipeAndPassReceiver(), std::move(metadata));
  run_loop.Run();
}

}  // namespace web
