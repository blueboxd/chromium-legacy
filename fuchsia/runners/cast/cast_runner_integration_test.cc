// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>
#include <lib/zx/channel.h>

#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/fuchsia/service_directory_client.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "fuchsia/base/agent_impl.h"
#include "fuchsia/base/fake_component_context.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/cast/fake_application_config_manager.h"
#include "fuchsia/runners/cast/test_api_bindings.h"
#include "fuchsia/runners/common/web_component.h"
#include "fuchsia/runners/common/web_content_runner.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace castrunner {

namespace {

const char kTestServerRoot[] =
    FILE_PATH_LITERAL("fuchsia/runners/cast/testdata");

void ComponentErrorHandler(zx_status_t status) {
  ZX_LOG(ERROR, status) << "Component launch failed";
  ADD_FAILURE();
}

std::vector<uint8_t> StringToUnsignedVector(base::StringPiece str) {
  const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(str.data());
  return std::vector<uint8_t>(raw_data, raw_data + str.length());
}

class FakeAdditionalHeadersProvider
    : public fuchsia::web::AdditionalHeadersProvider {
 public:
  FakeAdditionalHeadersProvider(base::fuchsia::ServiceDirectory* directory)
      : binding_(directory, this) {}
  ~FakeAdditionalHeadersProvider() override = default;

 private:
  void GetHeaders(GetHeadersCallback callback) override {
    std::vector<fuchsia::net::http::Header> headers;
    fuchsia::net::http::Header header;
    header.name = StringToUnsignedVector("Test");
    header.value = StringToUnsignedVector("Value");
    headers.push_back(std::move(header));
    callback(std::move(headers), 0);
  }

  const base::fuchsia::ScopedServiceBinding<
      fuchsia::web::AdditionalHeadersProvider>
      binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeAdditionalHeadersProvider);
};

class FakeComponentState : public cr_fuchsia::AgentImpl::ComponentStateBase {
 public:
  FakeComponentState(
      base::StringPiece component_url,
      chromium::cast::ApplicationConfigManager* app_config_manager,
      chromium::cast::ApiBindings* bindings_manager)
      : ComponentStateBase(component_url),
        app_config_binding_(service_directory(), app_config_manager),
        additional_headers_provider_(
            std::make_unique<FakeAdditionalHeadersProvider>(
                service_directory())) {
    if (bindings_manager) {
      bindings_manager_binding_ = std::make_unique<
          base::fuchsia::ScopedServiceBinding<chromium::cast::ApiBindings>>(
          service_directory(), bindings_manager);
    }
  }
  ~FakeComponentState() override {
    if (on_delete_)
      std::move(on_delete_).Run();
  }

  void set_on_delete(base::OnceClosure on_delete) {
    on_delete_ = std::move(on_delete);
  }

 protected:
  const base::fuchsia::ScopedServiceBinding<
      chromium::cast::ApplicationConfigManager>
      app_config_binding_;
  std::unique_ptr<
      base::fuchsia::ScopedServiceBinding<chromium::cast::ApiBindings>>
      bindings_manager_binding_;
  std::unique_ptr<FakeAdditionalHeadersProvider> additional_headers_provider_;
  base::OnceClosure on_delete_;

  DISALLOW_COPY_AND_ASSIGN(FakeComponentState);
};

}  // namespace

class CastRunnerIntegrationTest : public testing::Test {
 public:
  CastRunnerIntegrationTest()
      : run_timeout_(
            TestTimeouts::action_timeout(),
            base::MakeExpectedNotRunClosure(FROM_HERE, "Run() timed out.")) {
    // Create a new test ServiceDirectory, and ServiceDirectoryClient connected
    // to it, for tests to use to drive the CastRunner.
    fidl::InterfaceHandle<fuchsia::io::Directory> directory;
    public_services_ = std::make_unique<base::fuchsia::ServiceDirectory>(
        directory.NewRequest());

    // Create the CastRunner, published into |test_services_|.
    cast_runner_ = std::make_unique<CastRunner>(
        public_services_.get(), WebContentRunner::CreateIncognitoWebContext());

    // Connect to the CastRunner's fuchsia.sys.Runner interface.
    base::fuchsia::ServiceDirectoryClient public_directory_client(
        std::move(directory));
    cast_runner_ptr_ =
        public_directory_client.ConnectToService<fuchsia::sys::Runner>();
    cast_runner_ptr_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "CastRunner closed channel.";
      ADD_FAILURE();
    });
  }

  void SetUp() override {
    test_server_.ServeFilesFromSourceDirectory(kTestServerRoot);
    net::test_server::RegisterDefaultHandlers(&test_server_);
    ASSERT_TRUE(test_server_.Start());
  }

  void TearDown() override {
    // Disconnect the CastRunner & let things tear-down.
    cast_runner_ptr_.Unbind();
    base::RunLoop().RunUntilIdle();
  }

  fuchsia::sys::ComponentControllerPtr StartCastComponent(
      base::StringPiece component_url) {
    DCHECK(!component_state_);

    // Create a ServiceDirectory and publish the ComponentContext into it.
    fidl::InterfaceHandle<fuchsia::io::Directory> directory;
    component_services_ = std::make_unique<base::fuchsia::ServiceDirectory>(
        directory.NewRequest());
    component_context_ = std::make_unique<cr_fuchsia::FakeComponentContext>(
        base::BindRepeating(&CastRunnerIntegrationTest::OnComponentConnect,
                            base::Unretained(this)),
        component_services_.get(), component_url);

    // Configure the Runner, including a service directory channel to publish
    // services to.
    fuchsia::sys::StartupInfo startup_info;
    startup_info.launch_info.url = component_url.as_string();

    // Place the ServiceDirectory in the |flat_namespace|.
    startup_info.flat_namespace.paths.emplace_back(
        base::fuchsia::kServiceDirectoryPath);
    startup_info.flat_namespace.directories.emplace_back(
        directory.TakeChannel());

    fuchsia::sys::Package package;
    package.resolved_url = component_url.as_string();

    fuchsia::sys::ComponentControllerPtr component_controller;
    cast_runner_ptr_->StartComponent(std::move(package),
                                     std::move(startup_info),
                                     component_controller.NewRequest());

    EXPECT_TRUE(component_controller.is_bound());
    return component_controller;
  }

 protected:
  std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> OnComponentConnect(
      base::StringPiece component_url) {
    auto component_state = std::make_unique<FakeComponentState>(
        component_url, &app_config_manager_,
        (provide_api_bindings_ ? &api_bindings_ : nullptr));
    component_state_ = component_state.get();
    return component_state;
  }

  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;
  base::MessageLoopForIO message_loop_;
  net::EmbeddedTestServer test_server_;

  // Returns fake Cast application information to the CastRunner.
  FakeApplicationConfigManager app_config_manager_;

  TestApiBindings api_bindings_;

  // If set, publishes an ApiBindings service from the Agent.
  bool provide_api_bindings_ = true;

  // Incoming service directory, ComponentContext and per-component state.
  std::unique_ptr<base::fuchsia::ServiceDirectory> component_services_;
  std::unique_ptr<cr_fuchsia::FakeComponentContext> component_context_;
  FakeComponentState* component_state_ = nullptr;

  // ServiceDirectory into which the CastRunner will publish itself.
  std::unique_ptr<base::fuchsia::ServiceDirectory> public_services_;

  std::unique_ptr<CastRunner> cast_runner_;
  fuchsia::sys::RunnerPtr cast_runner_ptr_;

  DISALLOW_COPY_AND_ASSIGN(CastRunnerIntegrationTest);
};

// A basic integration test ensuring a basic cast request launches the right
// URL in the Chromium service.
TEST_F(CastRunnerIntegrationTest, BasicRequest) {
  const char kBlankAppId[] = "00000000";
  const char kBlankAppPath[] = "/defaultresponse";
  app_config_manager_.AddAppMapping(kBlankAppId,
                                    test_server_.GetURL(kBlankAppPath));

  // Launch the test-app component.
  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent(base::StringPrintf("cast:%s", kBlankAppId));
  component_controller.set_error_handler(&ComponentErrorHandler);

  // Access the NavigationController from the WebComponent. The test will hang
  // here if no WebComponent was created.
  fuchsia::web::NavigationControllerPtr nav_controller;
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<WebComponent*> web_component(
        run_loop.QuitClosure());
    cast_runner_->GetWebComponentForTest(web_component.GetReceiveCallback());
    run_loop.Run();
    ASSERT_NE(*web_component, nullptr);
    (*web_component)
        ->frame()
        ->GetNavigationController(nav_controller.NewRequest());
  }

  // Ensure the NavigationState has the expected URL.
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::NavigationState> nav_entry(
        run_loop.QuitClosure());
    nav_controller->GetVisibleEntry(
        cr_fuchsia::CallbackToFitFunction(nav_entry.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_TRUE(nav_entry->has_url());
    EXPECT_EQ(nav_entry->url(), test_server_.GetURL(kBlankAppPath).spec());
  }

  // Verify that the component is torn down when |component_controller| is
  // unbound.
  base::RunLoop run_loop;
  component_state_->set_on_delete(run_loop.QuitClosure());
  component_controller.Unbind();
  run_loop.Run();
}

TEST_F(CastRunnerIntegrationTest, ApiBindings) {
  provide_api_bindings_ = true;
  const char kBlankAppId[] = "00000000";
  const char kBlankAppPath[] = "/echo.html";
  app_config_manager_.AddAppMapping(kBlankAppId,
                                    test_server_.GetURL(kBlankAppPath));

  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(cr_fuchsia::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('echoService');"));
  binding_list.emplace_back(std::move(echo_binding));
  api_bindings_.set_bindings(std::move(binding_list));

  // Launch the test-app component.
  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent(base::StringPrintf("cast:%s", kBlankAppId));
  component_controller.set_error_handler(&ComponentErrorHandler);

  fuchsia::web::MessagePortPtr port =
      api_bindings_.RunUntilMessagePortReceived("echoService").Bind();

  fuchsia::web::WebMessage message;
  message.set_data(cr_fuchsia::MemBufferFromString("ping"));
  port->PostMessage(std::move(message),
                    [](fuchsia::web::MessagePort_PostMessage_Result result) {
                      EXPECT_TRUE(result.is_response());
                    });

  base::RunLoop response_loop;
  cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> response(
      response_loop.QuitClosure());
  port->ReceiveMessage(
      cr_fuchsia::CallbackToFitFunction(response.GetReceiveCallback()));
  response_loop.Run();

  std::string response_string;
  EXPECT_TRUE(
      cr_fuchsia::StringFromMemBuffer(response->data(), &response_string));
  EXPECT_EQ("ack ping", response_string);
}

TEST_F(CastRunnerIntegrationTest, IncorrectCastAppId) {
  // Launch the a component with an invalid Cast app Id.
  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent("cast:99999999");
  component_controller.set_error_handler(&ComponentErrorHandler);

  // Run the loop until the ComponentController is dropped, or a WebComponent is
  // created.
  base::RunLoop run_loop;
  component_controller.set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  cr_fuchsia::ResultReceiver<WebComponent*> web_component(
      run_loop.QuitClosure());
  cast_runner_->GetWebComponentForTest(web_component.GetReceiveCallback());
  run_loop.Run();
  EXPECT_FALSE(web_component.has_value());
}

TEST_F(CastRunnerIntegrationTest, AdditionalHeadersProvider) {
  const char kEchoAppId[] = "00000000";
  const char kEchoAppPath[] = "/echoheader?Test";
  const GURL echo_app_url = test_server_.GetURL(kEchoAppPath);
  app_config_manager_.AddAppMapping(kEchoAppId, echo_app_url);

  // Launch the test-app component.
  fuchsia::sys::ComponentControllerPtr component_controller =
      StartCastComponent(base::StringPrintf("cast:%s", kEchoAppId));
  component_controller.set_error_handler(&ComponentErrorHandler);

  WebComponent* web_component = nullptr;
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<WebComponent*> web_component_receiver(
        run_loop.QuitClosure());
    cast_runner_->GetWebComponentForTest(
        web_component_receiver.GetReceiveCallback());
    run_loop.Run();
    ASSERT_NE(*web_component_receiver, nullptr);
    web_component = *web_component_receiver;
  }

  // Bind a TestNavigationListener to the Frame.
  cr_fuchsia::TestNavigationListener navigation_listener;
  fidl::Binding<fuchsia::web::NavigationEventListener>
      navigation_listener_binding(&navigation_listener);
  web_component->frame()->SetNavigationEventListener(
      navigation_listener_binding.NewBinding());
  navigation_listener.RunUntilUrlEquals(echo_app_url);

  // Check the header was properly set.
  base::Optional<base::Value> result = cr_fuchsia::ExecuteJavaScript(
      web_component->frame(), "document.body.innerText");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(result->GetString(), "Value");
}

}  // namespace castrunner
