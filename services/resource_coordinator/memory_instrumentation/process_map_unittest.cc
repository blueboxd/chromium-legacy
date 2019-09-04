// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "services/resource_coordinator/memory_instrumentation/process_map.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

using RunningServiceInfoPtr = service_manager::mojom::RunningServiceInfoPtr;
using ServiceManagerListenerRequest =
    service_manager::mojom::ServiceManagerListenerRequest;
using testing::ElementsAre;

service_manager::Identity MakeFakeId(const std::string& service_name) {
  // The details of this Identity don't really matter. Name is used for test
  // expectations, and in general the last token should be unique, hence the
  // randomization.
  return service_manager::Identity{service_name, base::Token{1, 1},
                                   base::Token{}, base::Token::CreateRandom()};
}

RunningServiceInfoPtr MakeTestServiceInfo(
    const service_manager::Identity& identity,
    uint32_t pid) {
  RunningServiceInfoPtr info(service_manager::mojom::RunningServiceInfo::New());
  info->identity = identity;
  info->pid = pid;
  return info;
}

TEST(ProcessMapTest, PresentInInit) {
  // Add a dummy entry so that the actual |ids| indexes are 1-based.
  std::vector<service_manager::Identity> ids{MakeFakeId("id0")};
  std::vector<RunningServiceInfoPtr> instances;
  ProcessMap process_map(nullptr);
  for (uint32_t i = 1; i <= 3; i++) {
    ids.push_back(MakeFakeId(base::StringPrintf("id%d", i)));
    instances.push_back(MakeTestServiceInfo(ids.back(), i /* pid */));
  }
  process_map.OnInit(std::move(instances));
  EXPECT_EQ(static_cast<base::ProcessId>(1), process_map.GetProcessId(ids[1]));
  EXPECT_EQ(static_cast<base::ProcessId>(2), process_map.GetProcessId(ids[2]));
  EXPECT_EQ(static_cast<base::ProcessId>(3), process_map.GetProcessId(ids[3]));

  process_map.OnServiceStopped(ids[1]);
  EXPECT_EQ(base::kNullProcessId, process_map.GetProcessId(ids[1]));
  EXPECT_EQ(static_cast<base::ProcessId>(3), process_map.GetProcessId(ids[3]));
}

TEST(ProcessMapTest, NullsInInitIgnored) {
  ProcessMap process_map(nullptr);
  service_manager::Identity id1 = MakeFakeId("id1");
  service_manager::Identity id2 = MakeFakeId("id2");

  std::vector<RunningServiceInfoPtr> instances;
  instances.push_back(MakeTestServiceInfo(id1, base::kNullProcessId));
  instances.push_back(MakeTestServiceInfo(id2, 2));

  process_map.OnInit(std::move(instances));
  EXPECT_EQ(base::kNullProcessId, process_map.GetProcessId(id1));
  EXPECT_EQ(static_cast<base::ProcessId>(2), process_map.GetProcessId(id2));

  process_map.OnServicePIDReceived(id1, 1 /* pid */);
  EXPECT_EQ(static_cast<base::ProcessId>(1), process_map.GetProcessId(id1));
  EXPECT_EQ(static_cast<base::ProcessId>(2), process_map.GetProcessId(id2));
}

TEST(ProcessMapTest, TypicalCase) {
  ProcessMap process_map(nullptr);
  service_manager::Identity id1 = MakeFakeId("id1");
  EXPECT_EQ(base::kNullProcessId, process_map.GetProcessId(id1));
  process_map.OnInit(std::vector<RunningServiceInfoPtr>());
  EXPECT_EQ(base::kNullProcessId, process_map.GetProcessId(id1));

  process_map.OnServiceCreated(MakeTestServiceInfo(id1, 1 /* pid */));
  process_map.OnServiceStarted(id1, 1 /* pid */);
  EXPECT_EQ(base::kNullProcessId, process_map.GetProcessId(id1));

  process_map.OnServicePIDReceived(id1, 1 /* pid */);
  EXPECT_EQ(static_cast<base::ProcessId>(1), process_map.GetProcessId(id1));

  // Adding a separate service with a different identity should have no effect
  // on the first identity registered.
  service_manager::Identity id2 = MakeFakeId("id2");
  process_map.OnServiceCreated(MakeTestServiceInfo(id2, 2 /* pid */));
  process_map.OnServicePIDReceived(id2, 2 /* pid */);
  EXPECT_EQ(static_cast<base::ProcessId>(1), process_map.GetProcessId(id1));
  EXPECT_EQ(static_cast<base::ProcessId>(2), process_map.GetProcessId(id2));
  std::map<base::ProcessId, std::vector<std::string>> pid_to_names =
      process_map.ComputePidToServiceNamesMap();
  EXPECT_EQ(2u, pid_to_names.size());
  EXPECT_THAT(pid_to_names[static_cast<base::ProcessId>(1)],
              ElementsAre("id1"));
  EXPECT_THAT(pid_to_names[static_cast<base::ProcessId>(2)],
              ElementsAre("id2"));

  // Once the service is stopped, searching for its id should return a null pid.
  process_map.OnServiceStopped(id1);
  EXPECT_EQ(base::kNullProcessId, process_map.GetProcessId(id1));
  EXPECT_EQ(static_cast<base::ProcessId>(2), process_map.GetProcessId(id2));

  // Unknown identities return a null pid.
  service_manager::Identity id3 = MakeFakeId("id3");
  EXPECT_EQ(base::kNullProcessId, process_map.GetProcessId(id3));

  // Can have multiple identities under same pid.
  service_manager::Identity id4 = MakeFakeId("id4");
  process_map.OnServiceCreated(MakeTestServiceInfo(id4, 2 /* pid */));
  process_map.OnServicePIDReceived(id4, 2 /* pid */);

  // Can get the pid for both, and the computed service map should have both.
  EXPECT_EQ(static_cast<base::ProcessId>(2), process_map.GetProcessId(id2));
  EXPECT_EQ(static_cast<base::ProcessId>(2), process_map.GetProcessId(id4));

  std::map<base::ProcessId, std::vector<std::string>> pid_to_names2 =
      process_map.ComputePidToServiceNamesMap();
  EXPECT_EQ(1u, pid_to_names2.size());
  EXPECT_THAT(pid_to_names2[static_cast<base::ProcessId>(2)],
              ElementsAre("id2", "id4"));
}

}  // namespace memory_instrumentation
