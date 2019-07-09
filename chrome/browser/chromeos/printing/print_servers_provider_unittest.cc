// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_servers_provider.h"

#include <string>
#include <vector>

#include "chrome/browser/chromeos/printing/print_server.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

// An example of configuration file with print servers.
constexpr char kPrintServersPolicyJson1[] = R"json(
[
  {
    "display_name": "MyPrintServer",
    "url": "ipp://192.168.1.5"
  }, {
    "display_name": "Server API",
    "url":"ipps://print-server.intra.example.com:443/ipp/cl2k4"
  }, {
    "display_name": "YaLP",
    "url": "http://192.168.1.8/bleble/print"
  }
])json";

// Corresponding vector with PrintServers.
const std::vector<PrintServer> kPrintServersPolicyData1 = {
    {GURL("http://192.168.1.5"), "MyPrintServer"},
    {GURL("https://print-server.intra.example.com:443/ipp/cl2k4"),
     "Server API"},
    {GURL("http://192.168.1.8/bleble/print"), "YaLP"}};

// A different configuration file with print servers.
constexpr char kPrintServersPolicyJson2[] = R"json(
[
  {
    "display_name": "CUPS",
    "url": "ipp://192.168.1.15"
  }
])json";

// Corresponding vector with PrintServers.
const std::vector<PrintServer> kPrintServersPolicyData2 = {
    {GURL("http://192.168.1.15"), "CUPS"}};

// Another configuration file with print servers, this time with invalid URLs.
constexpr char kPrintServersPolicyJson3[] = R"json(
[
  {
    "display_name": "server_1",
    "url": "ipp://aaa.bbb.ccc:666/xx"
  }, {
    "display_name": "server_2",
    "url":"ipps:/print.server.intra.example.com:443z/ipp"
  }, {
    "display_name": "server_3",
    "url": "file://192.168.1.8/bleble/print"
  }, {
    "display_name": "server_4",
    "url": "http://aaa.bbb.ccc:666/xx"
  }, {
    "display_name": "server_5",
    "url": "\n \t ipps://aaa.bbb.ccc:666/yy"
  }, {
    "display_name": "server_6",
    "url":"ipps:/print.server^.intra.example.com/ipp"
  }
])json";

// Corresponding vector with PrintServers. Only the first record is included,
// because other ones are invalid:
// server_2 - invalid URL - invalid port number
// server_3 - unsupported scheme
// server_4 - duplicate of server_1
// server_5 - leading whitespaces, they should be trimmed
// server_6 - invalid URL - forbidden character
const std::vector<PrintServer> kPrintServersPolicyData3 = {
    {GURL("http://aaa.bbb.ccc:666/xx"), "server_1"},
    {GURL("https://aaa.bbb.ccc:666/yy"), "server_5"}};

// Observer that stores all its calls.
class TestObserver : public PrintServersProvider::Observer {
 public:
  struct ObserverCall {
    bool complete;
    std::vector<PrintServer> servers;
    ObserverCall(bool complete, const std::vector<PrintServer>& servers)
        : complete(complete), servers(servers) {}
  };

  ~TestObserver() override = default;

  // Callback from PrintServersProvider::Observer.
  void OnServersChanged(bool complete,
                        const std::vector<PrintServer>& servers) override {
    calls_.emplace_back(complete, servers);
  }

  // Returns history of all calls to OnServersChanged(...).
  const std::vector<ObserverCall>& GetCalls() const { return calls_; }

 private:
  // history of all callbacks
  std::vector<ObserverCall> calls_;
};

class PrintServersProviderTest : public testing::Test {
 public:
  PrintServersProviderTest()
      : external_servers_(PrintServersProvider::Create()) {}

 protected:
  // everything must be called on Chrome_UIThread
  content::TestBrowserThreadBundle scoped_task_environment_;
  std::unique_ptr<PrintServersProvider> external_servers_;
};

// Verify that the object can be destroyed while parsing is in progress.
TEST_F(PrintServersProviderTest, DestructionIsSafe) {
  {
    std::unique_ptr<PrintServersProvider> servers =
        PrintServersProvider::Create();
    servers->SetData(std::make_unique<std::string>(kPrintServersPolicyJson1));
    // Data is valid.  Computation is proceeding.
  }
  // servers is out of scope.  Destructor has run.  Pump the message queue to
  // see if anything strange happens.
  scoped_task_environment_.RunUntilIdle();
}

// Verify that we're initially unset and empty.
// After initialization "complete" flags = false.
TEST_F(PrintServersProviderTest, InitialConditions) {
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  ASSERT_EQ(obs.GetCalls().size(), 1u);
  EXPECT_EQ(obs.GetCalls().back().complete, false);
  EXPECT_TRUE(obs.GetCalls().back().servers.empty());
  external_servers_->RemoveObserver(&obs);
}

// Verify two ClearData() calls.
// ClearData() sets empty list and "complete" flag = true.
TEST_F(PrintServersProviderTest, ClearData2) {
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  external_servers_->ClearData();
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  EXPECT_EQ(obs.GetCalls().back().complete, true);
  EXPECT_TRUE(obs.GetCalls().back().servers.empty());
  external_servers_->ClearData();
  // no changes, because observed object's state is the same
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  external_servers_->RemoveObserver(&obs);
}

// Verifies SetData().
// SetData(...) sets "complete" flag = false, then parse given data in the
// background and sets resultant list with "complete" flag = true.
TEST_F(PrintServersProviderTest, SetData) {
  auto blob1 = std::make_unique<std::string>(kPrintServersPolicyJson1);
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  external_servers_->SetData(std::move(blob1));
  // single call from AddObserver, since SetData(...) is not processed yet
  ASSERT_EQ(obs.GetCalls().size(), 1u);
  // make sure that SetData(...) is processed
  scoped_task_environment_.RunUntilIdle();
  // now the call from SetData(...) is there also
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  EXPECT_EQ(obs.GetCalls().back().complete, true);
  EXPECT_EQ(obs.GetCalls().back().servers, kPrintServersPolicyData1);
  external_servers_->RemoveObserver(&obs);
}

// Verify two SetData() calls.
TEST_F(PrintServersProviderTest, SetData2) {
  auto blob1 = std::make_unique<std::string>(kPrintServersPolicyJson1);
  auto blob2 = std::make_unique<std::string>(kPrintServersPolicyJson2);
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  external_servers_->SetData(std::move(blob1));
  // single call from AddObserver, since SetData(...) is not processed yet
  ASSERT_EQ(obs.GetCalls().size(), 1u);
  external_servers_->SetData(std::move(blob2));
  // no changes, because nothing was processed yet
  ASSERT_EQ(obs.GetCalls().size(), 1u);
  scoped_task_environment_.RunUntilIdle();
  // both calls from SetData(...) should be reported
  ASSERT_EQ(obs.GetCalls().size(), 3u);
  EXPECT_EQ(obs.GetCalls()[1].complete, false);
  EXPECT_EQ(obs.GetCalls()[1].servers, kPrintServersPolicyData1);
  EXPECT_EQ(obs.GetCalls()[2].complete, true);
  EXPECT_EQ(obs.GetCalls()[2].servers, kPrintServersPolicyData2);
  external_servers_->RemoveObserver(&obs);
}

// Verifies SetData() + ClearData() before SetData() completes.
TEST_F(PrintServersProviderTest, SetDataClearData) {
  auto blob1 = std::make_unique<std::string>(kPrintServersPolicyJson1);
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  external_servers_->SetData(std::move(blob1));
  // single call from AddObserver, since SetData(...) is not processed yet
  ASSERT_EQ(obs.GetCalls().size(), 1u);
  external_servers_->ClearData();
  // a call from ClearData() was added, SetData(...) is not processed yet
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  EXPECT_EQ(obs.GetCalls().back().complete, true);
  EXPECT_TRUE(obs.GetCalls().back().servers.empty());
  // process SetData(...)
  scoped_task_environment_.RunUntilIdle();
  // no changes, effects of SetData(...) were already replaced by ClearData()
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  external_servers_->RemoveObserver(&obs);
}

// Verifies ClearData() before AddObserver() + SetData() after.
TEST_F(PrintServersProviderTest, ClearDataSetData) {
  auto blob1 = std::make_unique<std::string>(kPrintServersPolicyJson1);
  TestObserver obs;
  external_servers_->ClearData();
  external_servers_->AddObserver(&obs);
  // single call from AddObserver, but with effects of ClearData()
  ASSERT_EQ(obs.GetCalls().size(), 1u);
  EXPECT_EQ(obs.GetCalls().back().complete, true);
  EXPECT_TRUE(obs.GetCalls().back().servers.empty());
  external_servers_->SetData(std::move(blob1));
  // SetData(...) is not completed, but generates a call switching "complete"
  // flag to false
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  EXPECT_EQ(obs.GetCalls().back().complete, false);
  EXPECT_TRUE(obs.GetCalls().back().servers.empty());
  // process SetData(...)
  scoped_task_environment_.RunUntilIdle();
  // next call with results from processed SetData(...)
  ASSERT_EQ(obs.GetCalls().size(), 3u);
  EXPECT_EQ(obs.GetCalls().back().complete, true);
  EXPECT_EQ(obs.GetCalls().back().servers, kPrintServersPolicyData1);
  external_servers_->RemoveObserver(&obs);
}

// Verify that invalid URLs are filtered out.
TEST_F(PrintServersProviderTest, InvalidURLs) {
  auto blob3 = std::make_unique<std::string>(kPrintServersPolicyJson3);
  TestObserver obs;
  external_servers_->AddObserver(&obs);
  external_servers_->SetData(std::move(blob3));
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ(obs.GetCalls().size(), 2u);
  EXPECT_EQ(obs.GetCalls().back().complete, true);
  EXPECT_EQ(obs.GetCalls().back().servers, kPrintServersPolicyData3);
  external_servers_->RemoveObserver(&obs);
}

}  // namespace
}  // namespace chromeos
