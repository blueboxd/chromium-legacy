// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/ip_endpoint.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/tools/quic/quic_transport_simple_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

// This file is placed tentively in content/browser/loader.
// TODO(yhirano): Convert tests in this file to web platform tests when they
// have a QuicTransport server.

namespace content {
namespace {

using base::ASCIIToUTF16;

class QuicTransportSimpleServerWithThread final {
 public:
  explicit QuicTransportSimpleServerWithThread(
      const std::vector<url::Origin>& origins)
      : origins_(origins) {}

  ~QuicTransportSimpleServerWithThread() {
    io_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<net::QuicTransportSimpleServer> server) {},
            std::move(server_)));

    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait_for_thread_join;
    io_thread_.reset();
  }

  void Start() {
    CHECK(!io_thread_);

    io_thread_ = std::make_unique<base::Thread>("QuicTransport server");
    base::Thread::Options thread_options;
    thread_options.message_pump_type = base::MessagePumpType::IO;
    CHECK(io_thread_->StartWithOptions(thread_options));
    CHECK(io_thread_->WaitUntilThreadStarted());

    base::WaitableEvent event;
    net::IPEndPoint server_address;
    io_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          server_ = std::make_unique<net::QuicTransportSimpleServer>(
              /*port=*/0, origins_,
              quic::test::crypto_test_utils::ProofSourceForTesting());
          const auto result = server_->Start();
          CHECK_EQ(EXIT_SUCCESS, result);
          server_address = server_->server_address();
          event.Signal();
        }));
    event.Wait();
    server_address_ = server_address;
  }

  const net::IPEndPoint& server_address() const { return server_address_; }

 private:
  const std::vector<url::Origin> origins_;
  net::IPEndPoint server_address_;

  std::unique_ptr<net::QuicTransportSimpleServer> server_;
  std::unique_ptr<base::Thread> io_thread_;
};

class QuicTransportTest : public ContentBrowserTest {
 public:
  QuicTransportTest() : server_({}) { server_.Start(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(
        switches::kOriginToForceQuicOn,
        base::StringPrintf("localhost:%d", server_.server_address().port()));
    command_line->AppendSwitch(switches::kEnableQuic);
    command_line->AppendSwitchASCII(switches::kQuicVersion,
                                    base::StringPrintf("h3-24"));
    // The value is calculated from net/data/ssl/certificates/quic-chain.pem.
    command_line->AppendSwitchASCII(
        network::switches::kIgnoreCertificateErrorsSPKIList,
        "I+ryIVl5ksb8KijTneC3y7z1wBFn5x35O5is9g5n/KM=");
  }

  bool WaitForTitle(const base::string16& expected_title,
                    const std::vector<base::string16> additional_titles) {
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);

    for (const auto& title : additional_titles) {
      title_watcher.AlsoWaitForTitle(title);
    }
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
    return expected_title == actual_title;
  }

  bool WaitForTitle(const base::string16& title) {
    return WaitForTitle(title, {});
  }

 protected:
  QuicTransportSimpleServerWithThread server_;
};

// TODO(crbug.com/1044513): Run this test on Windows.
#if defined(OS_WIN)
#define MAYBE_Echo DISABLED_Echo
#else
#define MAYBE_Echo Echo
#endif
IN_PROC_BROWSER_TEST_F(QuicTransportTest, MAYBE_Echo) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("Title Of Awesomeness")));

  ASSERT_TRUE(ExecuteScript(
      shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new QuicTransport('quic-transport:localhost:%d/echo');

      const writer = transport.sendDatagrams().getWriter();
      const reader = transport.receiveDatagrams().getReader();

      const data = new Uint8Array([65, 66, 67]);
      const id = setInterval(() => {
        writer.write(data);
      }, 10);

      const {done, value} = await reader.read();
      clearInterval(id);
      if (done) {
        throw Error('Got an unexpected DONE signal');
      }
      if (value.length !== 3 ||
          value[0] !== 65 || value[1] !== 66 || value[2] !== 67) {
        throw Error('Got invalid data');
      }
    }

    run().then(() => { document.title = 'PASS'; },
               (e) => { console.log(e); document.title = 'FAIL'; });
)JS",
                                  server_.server_address().port())));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("PASS"), {ASCIIToUTF16("FAIL")}));
}

IN_PROC_BROWSER_TEST_F(QuicTransportTest, ClientIndicationFailure) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("Title Of Awesomeness")));

  ASSERT_TRUE(ExecuteScript(
      shell(), base::StringPrintf(R"JS(
    async function run() {
      // The client indication fails because there is no resource /X
      // on the server.
      const transport = new QuicTransport('quic-transport:localhost:%d/X');

      try {
        await transport.closed;
      } catch (e) {
        return;
      }
      throw Error('closed should be rejected');
    }

    run().then(() => { document.title = 'PASS'; },
               (e) => { console.log(e); document.title = 'FAIL'; });
)JS",
                                  server_.server_address().port())));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("PASS"), {ASCIIToUTF16("FAIL")}));
}

}  // namespace
}  // namespace content
