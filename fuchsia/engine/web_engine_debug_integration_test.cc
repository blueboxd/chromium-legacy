// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/cpp/component_context.h>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/file_utils.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/test/task_environment.h"
#include "fuchsia/base/context_provider_test_connector.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_devtools_list_fetcher.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/test_debug_listener.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestServerRoot[] = FILE_PATH_LITERAL("fuchsia/engine/test/data");

base::Optional<base::Value> ReadConfigFile(base::FilePath config_file_path) {
  char data[4096];
  int length = base::ReadFile(config_file_path, data, 4096);
  if (length == -1)
    return base::nullopt;

  base::StringPiece json(data, length);
  return base::JSONReader::Read(base::StringPiece(json));
}

bool WriteConfigFile(base::FilePath config_file_path,
                     const base::StringPiece& config) {
  base::File config_file(config_file_path,
                         base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  if (config_file.error_details() != base::File::FILE_OK)
    return false;

  int result = config_file.Write(0, config.data(), config.size());
  return result != -1;
}

}  // namespace

class WebEngineDebugIntegrationTest : public testing::Test {
 public:
  WebEngineDebugIntegrationTest()
      : dev_tools_listener_binding_(&dev_tools_listener_) {}

  ~WebEngineDebugIntegrationTest() override = default;

  void SetUp() override {
    // Add an argument to WebEngine instance to distinguish it from other
    // instances that may be started by other tests.
    std::string test_arg =
        std::string("--test-name=") +
        testing::UnitTest::GetInstance()->current_test_info()->name();

    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(test_arg);

    web_context_provider_ = cr_fuchsia::ConnectContextProvider(
        web_engine_controller_.NewRequest(), command_line);
    web_context_provider_.set_error_handler(
        [](zx_status_t status) { ADD_FAILURE(); });

    WaitForWebEngine();

    // Enumerate all entries in /hub/c/context_provider.cmx to find WebEngine
    // instance with |test_arg|.
    base::FileEnumerator file_enum(
        base::FilePath("/hub/c/context_provider.cmx"), false,
        base::FileEnumerator::DIRECTORIES);

    for (auto dir = file_enum.Next(); !dir.empty(); dir = file_enum.Next()) {
      std::string args;
      if (!base::ReadFileToString(dir.Append("args"), &args)) {
        // WebEngine may shutdown while we are enumerating the directory, so
        // it's safe to ignore this error.
        continue;
      }

      if (args.find(test_arg) != std::string::npos) {
        // There should only one instance of WebEngine with |test_arg|.
        EXPECT_TRUE(web_engine_path_.empty());

        web_engine_path_ = dir;

        // Keep iterating to check that there are no other matching instances.
      }
    }

    // Check that we've found the WebEngine instance with |test_arg|.
    ASSERT_FALSE(web_engine_path_.empty());

    debug_dir_ = std::make_unique<sys::ServiceDirectory>(
        base::fuchsia::OpenDirectory(web_engine_path_.Append("out/debug")));
    debug_dir_->Connect(debug_.NewRequest());

    // Attach the DevToolsListener. EnableDevTools has an acknowledgement
    // callback so the listener will have been added after this call returns.
    debug_->EnableDevTools(dev_tools_listener_binding_.NewBinding());

    test_server_.ServeFilesFromSourceDirectory(kTestServerRoot);
    ASSERT_TRUE(test_server_.Start());
  }

 protected:
  void WaitForWebEngine() {
    // Create a throwaway web context to ensure the WebEngine process is
    // initialized and a Debug instance can be created. This is necessary
    // because the Debug service is not available on the debug directory until
    // after the WebEngine is fully initialized.
    fuchsia::web::CreateContextParams create_params;
    auto directory = base::fuchsia::OpenDirectory(
        base::FilePath(base::fuchsia::kServiceDirectoryPath));
    ASSERT_TRUE(directory.is_valid());
    create_params.set_service_directory(std::move(directory));

    fuchsia::web::ContextPtr web_context;
    web_context_provider_->Create(std::move(create_params),
                                  web_context.NewRequest());
    web_context.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });

    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<
        fuchsia::web::Context_GetRemoteDebuggingPort_Result>
        port_receiver(run_loop.QuitClosure());
    web_context->GetRemoteDebuggingPort(
        cr_fuchsia::CallbackToFitFunction(port_receiver.GetReceiveCallback()));
    run_loop.Run();

    // Sanity check.
    ASSERT_TRUE(port_receiver->is_err());
    ASSERT_EQ(port_receiver->err(),
              fuchsia::web::ContextError::REMOTE_DEBUGGING_PORT_NOT_OPENED);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  TestDebugListener dev_tools_listener_;
  fidl::Binding<fuchsia::web::DevToolsListener> dev_tools_listener_binding_;
  base::FilePath web_engine_path_;
  std::unique_ptr<sys::ServiceDirectory> debug_dir_;
  fuchsia::web::ContextProviderPtr web_context_provider_;
  fidl::InterfaceHandle<fuchsia::sys::ComponentController>
      web_engine_controller_;
  fuchsia::web::DebugSyncPtr debug_;

  base::OnceClosure on_url_fetch_complete_ack_;

  net::EmbeddedTestServer test_server_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineDebugIntegrationTest);
};

enum class UserModeDebugging { kEnabled = 0, kDisabled = 1 };

// Helper struct to intiialize all data necessary for a Context to create a
// Frame and navigate it to a specific URL.
struct TestContextAndFrame {
  explicit TestContextAndFrame(fuchsia::web::ContextProvider* context_provider,
                               UserModeDebugging user_mode_debugging,
                               std::string url) {
    // Create a Context, a Frame and navigate it to |url|.
    auto directory = base::fuchsia::OpenDirectory(
        base::FilePath(base::fuchsia::kServiceDirectoryPath));
    if (!directory.is_valid())
      return;

    fuchsia::web::CreateContextParams create_params;
    create_params.set_service_directory(std::move(directory));
    if (user_mode_debugging == UserModeDebugging::kEnabled)
      create_params.set_remote_debugging_port(0);
    context_provider->Create(std::move(create_params), context.NewRequest());
    context->CreateFrame(frame.NewRequest());
    frame->GetNavigationController(controller.NewRequest());
    if (!cr_fuchsia::LoadUrlAndExpectResponse(
            controller.get(), fuchsia::web::LoadUrlParams(), url)) {
      ADD_FAILURE();
      context.Unbind();
      frame.Unbind();
      controller.Unbind();
      return;
    }
  }
  ~TestContextAndFrame() = default;

  fuchsia::web::ContextPtr context;
  fuchsia::web::FramePtr frame;
  fuchsia::web::NavigationControllerPtr controller;

  DISALLOW_COPY_AND_ASSIGN(TestContextAndFrame);
};

// Test the Debug service is properly started and accessible.
TEST_F(WebEngineDebugIntegrationTest, DebugService) {
  std::string url = test_server_.GetURL("/title1.html").spec();
  TestContextAndFrame frame_data(web_context_provider_.get(),
                                 UserModeDebugging::kDisabled, url);
  ASSERT_TRUE(frame_data.context);

  // Test the debug information is correct.
  dev_tools_listener_.RunUntilNumberOfPortsIs(1u);

  base::Value devtools_list = cr_fuchsia::GetDevToolsListFromPort(
      *dev_tools_listener_.debug_ports().begin());
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetList().size(), 1u);

  base::Value* devtools_url = devtools_list.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), url);

  base::Value* devtools_title = devtools_list.GetList()[0].FindPath("title");
  ASSERT_TRUE(devtools_title->is_string());
  EXPECT_EQ(devtools_title->GetString(), "title 1");

  // Unbind the context and wait for the listener to no longer have any active
  // DevTools port.
  frame_data.context.Unbind();
  dev_tools_listener_.RunUntilNumberOfPortsIs(0);
}

TEST_F(WebEngineDebugIntegrationTest, MultipleDebugClients) {
  std::string url1 = test_server_.GetURL("/title1.html").spec();
  TestContextAndFrame frame_data1(web_context_provider_.get(),
                                  UserModeDebugging::kDisabled, url1);
  ASSERT_TRUE(frame_data1.context);

  // Test the debug information is correct.
  dev_tools_listener_.RunUntilNumberOfPortsIs(1u);
  uint16_t port1 = *dev_tools_listener_.debug_ports().begin();

  base::Value devtools_list1 = cr_fuchsia::GetDevToolsListFromPort(port1);
  ASSERT_TRUE(devtools_list1.is_list());
  EXPECT_EQ(devtools_list1.GetList().size(), 1u);

  base::Value* devtools_url1 = devtools_list1.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url1->is_string());
  EXPECT_EQ(devtools_url1->GetString(), url1);

  base::Value* devtools_title1 = devtools_list1.GetList()[0].FindPath("title");
  ASSERT_TRUE(devtools_title1->is_string());
  EXPECT_EQ(devtools_title1->GetString(), "title 1");

  // Connect a second Debug interface.
  fuchsia::web::DebugSyncPtr debug2;
  debug_dir_->Connect(debug2.NewRequest());
  TestDebugListener dev_tools_listener2;
  fidl::Binding<fuchsia::web::DevToolsListener> dev_tools_listener_binding2(
      &dev_tools_listener2);
  debug2->EnableDevTools(dev_tools_listener_binding2.NewBinding());

  // Create a second Context, a second Frame and navigate it to title2.html.
  std::string url2 = test_server_.GetURL("/title2.html").spec();
  TestContextAndFrame frame_data2(web_context_provider_.get(),
                                  UserModeDebugging::kDisabled, url2);
  ASSERT_TRUE(frame_data2.context);

  // Ensure each DevTools listener has the right information.
  dev_tools_listener_.RunUntilNumberOfPortsIs(2u);
  dev_tools_listener2.RunUntilNumberOfPortsIs(1u);

  uint16_t port2 = *dev_tools_listener2.debug_ports().begin();
  ASSERT_NE(port1, port2);
  ASSERT_NE(dev_tools_listener_.debug_ports().find(port2),
            dev_tools_listener_.debug_ports().end());

  base::Value devtools_list2 = cr_fuchsia::GetDevToolsListFromPort(port2);
  ASSERT_TRUE(devtools_list2.is_list());
  EXPECT_EQ(devtools_list2.GetList().size(), 1u);

  base::Value* devtools_url2 = devtools_list2.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url2->is_string());
  EXPECT_EQ(devtools_url2->GetString(), url2);

  base::Value* devtools_title2 = devtools_list2.GetList()[0].FindPath("title");
  ASSERT_TRUE(devtools_title2->is_string());
  EXPECT_EQ(devtools_title2->GetString(), "title 2");

  // Unbind the first Context, each listener should still have one open port.
  frame_data1.context.Unbind();
  dev_tools_listener_.RunUntilNumberOfPortsIs(1u);
  dev_tools_listener2.RunUntilNumberOfPortsIs(1u);

  // Unbind the second Context, no listener should have any open port.
  frame_data2.context.Unbind();
  dev_tools_listener_.RunUntilNumberOfPortsIs(0);
  dev_tools_listener2.RunUntilNumberOfPortsIs(0);
}

// Test the Debug service is accessible when the User service is requested.
TEST_F(WebEngineDebugIntegrationTest, DebugAndUserService) {
  std::string url = test_server_.GetURL("/title1.html").spec();
  TestContextAndFrame frame_data(web_context_provider_.get(),
                                 UserModeDebugging::kEnabled, url);
  ASSERT_TRUE(frame_data.context);

  dev_tools_listener_.RunUntilNumberOfPortsIs(1u);

  // Check we are getting the same port on both the debug and user APIs.
  base::RunLoop run_loop;
  cr_fuchsia::ResultReceiver<
      fuchsia::web::Context_GetRemoteDebuggingPort_Result>
      port_receiver(run_loop.QuitClosure());
  frame_data.context->GetRemoteDebuggingPort(
      cr_fuchsia::CallbackToFitFunction(port_receiver.GetReceiveCallback()));
  run_loop.Run();

  ASSERT_TRUE(port_receiver->is_response());
  uint16_t remote_debugging_port = port_receiver->response().port;
  ASSERT_EQ(remote_debugging_port, *dev_tools_listener_.debug_ports().begin());

  // Test the debug information is correct.
  base::Value devtools_list =
      cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetList().size(), 1u);

  base::Value* devtools_url = devtools_list.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), url);

  base::Value* devtools_title = devtools_list.GetList()[0].FindPath("title");
  ASSERT_TRUE(devtools_title->is_string());
  EXPECT_EQ(devtools_title->GetString(), "title 1");

  // Unbind the context and wait for the listener to no longer have any active
  // DevTools port.
  frame_data.context.Unbind();
  dev_tools_listener_.RunUntilNumberOfPortsIs(0);
}

// Checks the default and override configuration files in the debug directory
// behave properly.
// Due to potential side effects with other integration tests, these checks need
// to be kept in a single test.
TEST_F(WebEngineDebugIntegrationTest, ConfigOverride) {
  base::FilePath default_config_path =
      web_engine_path_.Append("out/debug/config-default.json");
  base::FilePath config_override_path =
      web_engine_path_.Append("out/debug/config-override.json");
  const std::string kInvalidValue = "[foo";
  const std::string kNonDictionayJson = "[\"foo\"]";
  const std::string kDisableJavaScriptConfig =
      "{\"command-line-args\":{\"blink-settings\":\"scriptEnabled=false\"}}";

  {
    // Read the default configuration. It should be a valid JSON dictionary.
    base::Optional<base::Value> default_config =
        ReadConfigFile(default_config_path);
    ASSERT_TRUE(default_config);
    EXPECT_TRUE(default_config->is_dict());

    // Check the default configuration is read-only.
    EXPECT_FALSE(WriteConfigFile(default_config_path, "foo"));
  }

  {
    // Read the original config override, it should be an empty dictionary.
    base::Optional<base::Value> config_override =
        ReadConfigFile(config_override_path);
    ASSERT_TRUE(config_override);
    EXPECT_TRUE(config_override->is_dict());
    EXPECT_TRUE(config_override->DictEmpty());
  }

  {
    // Attempt to write a non-JSON value to the config override and read the
    // value again, it should still be an empty dictionary.
    ASSERT_TRUE(WriteConfigFile(config_override_path, kInvalidValue));
    base::Optional<base::Value> config_override =
        ReadConfigFile(config_override_path);
    ASSERT_TRUE(config_override);
    EXPECT_TRUE(config_override->is_dict());
    EXPECT_TRUE(config_override->DictEmpty());
  }

  {
    // Attempt to write a non-dictionary JSON value to the config override and
    // read the value again, it should still be an empty dictionary.
    ASSERT_TRUE(WriteConfigFile(config_override_path, kNonDictionayJson));
    base::Optional<base::Value> config_override =
        ReadConfigFile(config_override_path);
    ASSERT_TRUE(config_override);
    EXPECT_TRUE(config_override->is_dict());
    EXPECT_TRUE(config_override->DictEmpty());
  }

  {
    // Write a dictionary JSON value to the config override. The file should
    // have been updated.
    ASSERT_TRUE(
        WriteConfigFile(config_override_path, kDisableJavaScriptConfig));
    base::Optional<base::Value> config_override =
        ReadConfigFile(config_override_path);
    ASSERT_TRUE(config_override);
    EXPECT_TRUE(config_override->is_dict());
    EXPECT_FALSE(config_override->DictEmpty());

    // Load a page and attempt to execute JavaScript. JavaScript execution
    // should fail and return a "null" value.
    std::string url = test_server_.GetURL("/title1.html").spec();
    TestContextAndFrame frame_data(web_context_provider_.get(),
                                   UserModeDebugging::kDisabled, url);

    base::Optional<base::Value> value =
        cr_fuchsia::ExecuteJavaScript(frame_data.frame.get(), "42;");
    ASSERT_TRUE(value);
    EXPECT_TRUE(value->is_none());
  }

  {
    // Clear the config override and read the value again, it should be an empty
    // dictionary.
    ASSERT_TRUE(WriteConfigFile(config_override_path, "{}"));
    base::Optional<base::Value> config_override =
        ReadConfigFile(config_override_path);
    ASSERT_TRUE(config_override);
    EXPECT_TRUE(config_override->is_dict());
    EXPECT_TRUE(config_override->DictEmpty());
  }
}
