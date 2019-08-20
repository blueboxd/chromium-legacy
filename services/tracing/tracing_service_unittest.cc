// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/public/mojom/tracing.mojom.h"
#include "services/tracing/test_util.h"
#include "services/tracing/tracing_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

class TracingServiceTest : public testing::Test {
 public:
  TracingServiceTest()
      : service_(
            test_connector_factory_.RegisterInstance(mojom::kServiceName)) {
    test_connector_factory_.set_ignore_unknown_service_requests(true);
  }
  ~TracingServiceTest() override {}

 protected:
  service_manager::Connector* connector() {
    return test_connector_factory_.GetDefaultConnector();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  service_manager::TestConnectorFactory test_connector_factory_;
  TracingService service_;

  DISALLOW_COPY_AND_ASSIGN(TracingServiceTest);
};

TEST_F(TracingServiceTest, TracingServiceInstantiate) {
  mojom::CoordinatorPtr coordinator;
  connector()->BindInterface(mojom::kServiceName,
                             mojo::MakeRequest(&coordinator));

  base::RunLoop tracing_started;
  coordinator->IsTracing(base::BindOnce(
      [](base::OnceClosure callback, bool is_tracing) {
        EXPECT_FALSE(is_tracing);
        std::move(callback).Run();
      },
      tracing_started.QuitClosure()));
  tracing_started.Run();
}

}  // namespace tracing
