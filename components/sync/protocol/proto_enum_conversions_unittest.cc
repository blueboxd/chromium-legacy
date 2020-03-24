// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/proto_enum_conversions.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

// Keep this file in sync with the .proto files in this directory.
class ProtoEnumConversionsTest : public testing::Test {};

template <class T>
void TestEnumStringFunction(T enum_min, T enum_max) {
  for (int i = enum_min; i <= enum_max; ++i) {
    const std::string& str = ProtoEnumToString(static_cast<T>(i));
    EXPECT_FALSE(str.empty());
  }
}

TEST_F(ProtoEnumConversionsTest, GetAppListItemTypeString) {
  TestEnumStringFunction(sync_pb::AppListSpecifics::AppListItemType_MIN,
                         sync_pb::AppListSpecifics::AppListItemType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetBrowserTypeString) {
  TestEnumStringFunction(sync_pb::SessionWindow::BrowserType_MIN,
                         sync_pb::SessionWindow::BrowserType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetPageTransitionString) {
  TestEnumStringFunction(sync_pb::SyncEnums::PageTransition_MIN,
                         sync_pb::SyncEnums::PageTransition_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetPageTransitionQualifierString) {
  TestEnumStringFunction(sync_pb::SyncEnums::PageTransitionRedirectType_MIN,
                         sync_pb::SyncEnums::PageTransitionRedirectType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetWifiConfigurationSecurityTypeString) {
  TestEnumStringFunction(sync_pb::WifiConfigurationSpecifics::SecurityType_MIN,
                         sync_pb::WifiConfigurationSpecifics::SecurityType_MAX);
}

TEST_F(ProtoEnumConversionsTest,
       GetWifiConfigurationAutomaticallyConnectOptionString) {
  TestEnumStringFunction(
      sync_pb::WifiConfigurationSpecifics::AutomaticallyConnectOption_MIN,
      sync_pb::WifiConfigurationSpecifics::AutomaticallyConnectOption_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetWifiConfigurationIsPreferredOptionString) {
  TestEnumStringFunction(
      sync_pb::WifiConfigurationSpecifics::IsPreferredOption_MIN,
      sync_pb::WifiConfigurationSpecifics::IsPreferredOption_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetWifiConfigurationMeteredOptionString) {
  TestEnumStringFunction(
      sync_pb::WifiConfigurationSpecifics::MeteredOption_MIN,
      sync_pb::WifiConfigurationSpecifics::MeteredOption_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetWifiConfigurationProxyOptionString) {
  TestEnumStringFunction(
      sync_pb::WifiConfigurationSpecifics::ProxyConfiguration::ProxyOption_MIN,
      sync_pb::WifiConfigurationSpecifics::ProxyConfiguration::ProxyOption_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetUpdatesSourceString) {
  TestEnumStringFunction(sync_pb::GetUpdatesCallerInfo::GetUpdatesSource_MIN,
                         sync_pb::GetUpdatesCallerInfo::PERIODIC);
  TestEnumStringFunction(sync_pb::GetUpdatesCallerInfo::RETRY,
                         sync_pb::GetUpdatesCallerInfo::GetUpdatesSource_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetUpdatesOriginString) {
  // This enum has rather scattered values, so we need multiple ranges.
  TestEnumStringFunction(sync_pb::SyncEnums::GetUpdatesOrigin_MIN,
                         sync_pb::SyncEnums::UNKNOWN_ORIGIN);
  TestEnumStringFunction(sync_pb::SyncEnums::PERIODIC,
                         sync_pb::SyncEnums::PERIODIC);
  TestEnumStringFunction(sync_pb::SyncEnums::NEWLY_SUPPORTED_DATATYPE,
                         sync_pb::SyncEnums::RECONFIGURATION);
  TestEnumStringFunction(sync_pb::SyncEnums::GU_TRIGGER,
                         sync_pb::SyncEnums::GetUpdatesOrigin_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetResponseTypeString) {
  TestEnumStringFunction(sync_pb::CommitResponse::ResponseType_MIN,
                         sync_pb::CommitResponse::ResponseType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetErrorTypeString) {
  // We have a gap, so we need to do two ranges.
  TestEnumStringFunction(sync_pb::SyncEnums::ErrorType_MIN,
                         sync_pb::SyncEnums::MIGRATION_DONE);
  TestEnumStringFunction(sync_pb::SyncEnums::UNKNOWN,
                         sync_pb::SyncEnums::ErrorType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetActionString) {
  TestEnumStringFunction(sync_pb::SyncEnums::Action_MIN,
                         sync_pb::SyncEnums::Action_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetConsentStatusString) {
  TestEnumStringFunction(sync_pb::UserConsentTypes::CONSENT_STATUS_UNSPECIFIED,
                         sync_pb::UserConsentTypes::GIVEN);
}

}  // namespace
}  // namespace syncer
