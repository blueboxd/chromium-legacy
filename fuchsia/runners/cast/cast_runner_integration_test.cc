// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/modular/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>
#include <lib/zx/channel.h>

#include "base/base_paths_fuchsia.h"
#include "base/callback_helpers.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "fuchsia/base/agent_impl.h"
#include "fuchsia/base/context_provider_test_connector.h"
#include "fuchsia/base/fake_component_context.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/fuchsia_dir_scheme.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/string_util.h"
#include "fuchsia/base/test_devtools_list_fetcher.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/base/url_request_rewrite_test_util.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/cast/fake_application_config_manager.h"
#include "fuchsia/runners/cast/test_api_bindings.h"
#include "fuchsia/runners/common/web_component.h"
#include "fuchsia/runners/common/web_content_runner.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAppId[] = "00000000";

constexpr char kBlankAppUrl[] = "/defaultresponse";
constexpr char kEchoHeaderPath[] = "/echoheader?Test";
constexpr char kEchoAppPath[] = "/echo.html";

constexpr char kTestServerRoot[] =
    FILE_PATH_LITERAL("fuchsia/runners/cast/testdata");

constexpr char kDummyAgentUrl[] =
    "fuchsia-pkg://fuchsia.com/dummy_agent#meta/dummy_agent.cmx";

void ComponentErrorHandler(zx_status_t status) {
  ZX_LOG(ERROR, status) << "Component launch failed";
  ADD_FAILURE();
}

// Helper used to ensure that cr_fuchsia::RegisterFuchsiaDirScheme() is called
// once per process to register fuchsia-dir scheme. In cast_runner this function
// is called in main.cc, but that code is not executed in
// cast_runner_integration_tests.
//
// TODO(crbug.com/1062351): Update the tests to start cast_runner component
// instead of creating CastRunner in process. Then remove this function.
void EnsureFuchsiaDirSchemeInitialized() {
  class SchemeInitializer {
   public:
    SchemeInitializer() { cr_fuchsia::RegisterFuchsiaDirScheme(); }
  };
  static SchemeInitializer initializer;
}

class FakeUrlRequestRewriteRulesProvider
    : public chromium::cast::UrlRequestRewriteRulesProvider {
 public:
  FakeUrlRequestRewriteRulesProvider() = default;
  ~FakeUrlRequestRewriteRulesProvider() override = default;
  FakeUrlRequestRewriteRulesProvider(
      const FakeUrlRequestRewriteRulesProvider&) = delete;
  FakeUrlRequestRewriteRulesProvider& operator=(
      const FakeUrlRequestRewriteRulesProvider&) = delete;

 private:
  void GetUrlRequestRewriteRules(
      GetUrlRequestRewriteRulesCallback callback) override {
    // Only send the rules once. They do not expire
    if (rules_sent_)
      return;
    rules_sent_ = true;

    std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
    rewrites.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test", "Value"));
    fuchsia::web::UrlRequestRewriteRule rule;
    rule.set_rewrites(std::move(rewrites));
    std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
    rules.push_back(std::move(rule));
    callback(std::move(rules));
  }

  bool rules_sent_ = false;
};

class FakeApplicationContext : public chromium::cast::ApplicationContext {
 public:
  FakeApplicationContext() = default;
  ~FakeApplicationContext() final = default;

  FakeApplicationContext(const FakeApplicationContext&) = delete;
  FakeApplicationContext& operator=(const FakeApplicationContext&) = delete;

  chromium::cast::ApplicationController* controller() {
    if (!controller_)
      return nullptr;

    return controller_.get();
  }

 private:
  // chromium::cast::ApplicationContext implementation.
  void GetMediaSessionId(GetMediaSessionIdCallback callback) final {
    callback(0);
  }
  void SetApplicationController(
      fidl::InterfaceHandle<chromium::cast::ApplicationController> controller)
      final {
    controller_ = controller.Bind();
  }

  chromium::cast::ApplicationControllerPtr controller_;
};

class FakeComponentState : public cr_fuchsia::AgentImpl::ComponentStateBase {
 public:
  FakeComponentState(
      base::StringPiece component_url,
      chromium::cast::ApplicationConfigManager* app_config_manager,
      chromium::cast::ApiBindings* bindings_manager,
      chromium::cast::UrlRequestRewriteRulesProvider*
          url_request_rules_provider)
      : ComponentStateBase(component_url),
        app_config_binding_(outgoing_directory(), app_config_manager),
        bindings_manager_binding_(outgoing_directory(), bindings_manager),
        context_binding_(outgoing_directory(), &application_context_) {
    if (url_request_rules_provider) {
      url_request_rules_provider_binding_.emplace(outgoing_directory(),
                                                  url_request_rules_provider);
    }
  }

  ~FakeComponentState() override {
    if (on_delete_)
      std::move(on_delete_).Run();
  }
  FakeComponentState(const FakeComponentState&) = delete;
  FakeComponentState& operator=(const FakeComponentState&) = delete;

  // Make outgoing_directory() public.
  using ComponentStateBase::outgoing_directory;

  FakeApplicationContext* application_context() {
    return &application_context_;
  }

  void set_on_delete(base::OnceClosure on_delete) {
    on_delete_ = std::move(on_delete);
  }

  void Disconnect() { DisconnectClientsAndTeardown(); }

  bool api_bindings_has_clients() {
    return bindings_manager_binding_.has_clients();
  }

  bool url_request_rules_provider_has_clients() {
    if (url_request_rules_provider_binding_) {
      return url_request_rules_provider_binding_->has_clients();
    }
    return false;
  }

 protected:
  const base::fuchsia::ScopedServiceBinding<
      chromium::cast::ApplicationConfigManager>
      app_config_binding_;
  const base::fuchsia::ScopedServiceBinding<chromium::cast::ApiBindings>
      bindings_manager_binding_;
  base::Optional<base::fuchsia::ScopedServiceBinding<
      chromium::cast::UrlRequestRewriteRulesProvider>>
      url_request_rules_provider_binding_;
  FakeApplicationContext application_context_;
  const base::fuchsia::ScopedServiceBinding<chromium::cast::ApplicationContext>
      context_binding_;
  base::OnceClosure on_delete_;
};

}  // namespace

class CastRunnerIntegrationTest : public testing::Test {
 public:
  CastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(fuchsia::web::ContextFeatureFlags::NETWORK) {}
  CastRunnerIntegrationTest(const CastRunnerIntegrationTest&) = delete;
  CastRunnerIntegrationTest& operator=(const CastRunnerIntegrationTest&) =
      delete;

  void TearDown() override {
    // Disconnect the CastRunner & let things tear-down.
    cast_runner_ptr_.Unbind();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  explicit CastRunnerIntegrationTest(
      fuchsia::web::ContextFeatureFlags feature_flags)
      : app_config_manager_binding_(&component_services_,
                                    &app_config_manager_) {
    EnsureFuchsiaDirSchemeInitialized();

    // Create the CastRunner, published into |outgoing_directory_|.
    fuchsia::web::CreateContextParams create_context_params;
    create_context_params.set_features(feature_flags);
    create_context_params.set_service_directory(base::fuchsia::OpenDirectory(
        base::FilePath(base::fuchsia::kServiceDirectoryPath)));
    CHECK(create_context_params.service_directory());

    const uint16_t kRemoteDebuggingAnyPort = 0;
    create_context_params.set_remote_debugging_port(kRemoteDebuggingAnyPort);
    cast_runner_ = std::make_unique<CastRunner>(
        std::move(create_context_params), &outgoing_directory_);

    cast_runner_->SetContextProviderForTest(cr_fuchsia::ConnectContextProvider(
        context_provider_controller_.NewRequest()));

    // Connect to the CastRunner's fuchsia.sys.Runner interface.
    fidl::InterfaceHandle<fuchsia::io::Directory> directory;
    outgoing_directory_.GetOrCreateDirectory("svc")->Serve(
        fuchsia::io::OPEN_RIGHT_READABLE | fuchsia::io::OPEN_RIGHT_WRITABLE,
        directory.NewRequest().TakeChannel());
    sys::ServiceDirectory public_directory_client(std::move(directory));
    cast_runner_ptr_ = public_directory_client.Connect<fuchsia::sys::Runner>();
    cast_runner_ptr_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "CastRunner closed channel.";
      ADD_FAILURE();
    });

    test_server_.ServeFilesFromSourceDirectory(kTestServerRoot);
    net::test_server::RegisterDefaultHandlers(&test_server_);
    EXPECT_TRUE(test_server_.Start());
  }

  std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> OnComponentConnect(
      base::StringPiece component_url) {
    auto component_state = std::make_unique<FakeComponentState>(
        component_url, &app_config_manager_, &api_bindings_,
        &url_request_rewrite_rules_provider_);
    component_state_ = component_state.get();

    if (init_component_state_callback_)
      std::move(init_component_state_callback_).Run(component_state_);

    return component_state;
  }

  void RegisterAppWithTestData(GURL url) {
    fuchsia::web::ContentDirectoryProvider provider;
    provider.set_name("testdata");
    base::FilePath pkg_path;
    CHECK(base::PathService::Get(base::DIR_ASSETS, &pkg_path));
    provider.set_directory(base::fuchsia::OpenDirectory(
        pkg_path.AppendASCII("fuchsia/runners/cast/testdata")));
    std::vector<fuchsia::web::ContentDirectoryProvider> providers;
    providers.emplace_back(std::move(provider));

    auto app_config =
        FakeApplicationConfigManager::CreateConfig(kTestAppId, url);
    app_config.set_content_directories_for_isolated_application(
        std::move(providers));
    app_config_manager_.AddAppConfig(std::move(app_config));
  }

  void CreateComponentContextAndStartComponent() {
    auto component_url = base::StringPrintf("cast:%s", kTestAppId);
    CreateComponentContext(component_url);
    StartCastComponent(component_url);
    WaitComponentCreated();
  }

  void CreateComponentContext(const base::StringPiece& component_url) {
    component_context_ = std::make_unique<cr_fuchsia::FakeComponentContext>(
        base::BindRepeating(&CastRunnerIntegrationTest::OnComponentConnect,
                            base::Unretained(this)),
        &component_services_, component_url);
  }

  void StartCastComponent(base::StringPiece component_url) {
    // Configure the Runner, including a service directory channel to publish
    // services to.
    fidl::InterfaceHandle<fuchsia::io::Directory> directory;
    component_services_.GetOrCreateDirectory("svc")->Serve(
        fuchsia::io::OPEN_RIGHT_READABLE | fuchsia::io::OPEN_RIGHT_WRITABLE,
        directory.NewRequest().TakeChannel());
    fuchsia::sys::StartupInfo startup_info;
    startup_info.launch_info.url = component_url.as_string();

    fidl::InterfaceHandle<fuchsia::io::Directory> outgoing_directory;
    startup_info.launch_info.directory_request =
        outgoing_directory.NewRequest().TakeChannel();

    fidl::InterfaceHandle<::fuchsia::io::Directory> svc_directory;
    CHECK_EQ(fdio_service_connect_at(
                 outgoing_directory.channel().get(), "svc",
                 svc_directory.NewRequest().TakeChannel().release()),
             ZX_OK);

    component_services_client_ =
        std::make_unique<sys::ServiceDirectory>(std::move(svc_directory));

    // Place the ServiceDirectory in the |flat_namespace|.
    startup_info.flat_namespace.paths.emplace_back(
        base::fuchsia::kServiceDirectoryPath);
    startup_info.flat_namespace.directories.emplace_back(
        directory.TakeChannel());

    fuchsia::sys::Package package;
    package.resolved_url = component_url.as_string();

    cast_runner_ptr_->StartComponent(std::move(package),
                                     std::move(startup_info),
                                     component_controller_.NewRequest());
    component_controller_.set_error_handler(&ComponentErrorHandler);
  }

  void WaitComponentCreated() {
    EXPECT_FALSE(cast_component_);

    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<WebComponent*> component_receiver(
        run_loop.QuitClosure());
    cast_runner_->SetWebComponentCreatedCallbackForTest(
        base::AdaptCallbackForRepeating(
            component_receiver.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_NE(*component_receiver, nullptr);
    cast_component_ = reinterpret_cast<CastComponent*>(*component_receiver);
  }

  void WaitUrlAndTitle(const GURL& url, const std::string& title) {
    base::RunLoop run_loop;
    cr_fuchsia::TestNavigationListener listener;
    fidl::Binding<fuchsia::web::NavigationEventListener> listener_binding(
        &listener);
    cast_component_->frame()->SetNavigationEventListener(
        listener_binding.NewBinding());
    listener.RunUntilUrlAndTitleEquals(url, title);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;

  FakeApplicationConfigManager app_config_manager_;
  TestApiBindings api_bindings_;
  FakeUrlRequestRewriteRulesProvider url_request_rewrite_rules_provider_;

  // Incoming service directory, ComponentContext and per-component state.
  sys::OutgoingDirectory component_services_;
  base::fuchsia::ScopedServiceBinding<chromium::cast::ApplicationConfigManager>
      app_config_manager_binding_;
  std::unique_ptr<cr_fuchsia::FakeComponentContext> component_context_;
  fuchsia::sys::ComponentControllerPtr component_controller_;
  std::unique_ptr<sys::ServiceDirectory> component_services_client_;
  FakeComponentState* component_state_ = nullptr;
  CastComponent* cast_component_ = nullptr;

  base::OnceCallback<void(FakeComponentState*)> init_component_state_callback_;

  // ServiceDirectory into which the CastRunner will publish itself.
  sys::OutgoingDirectory outgoing_directory_;

  std::unique_ptr<CastRunner> cast_runner_;
  fuchsia::sys::RunnerPtr cast_runner_ptr_;
  fuchsia::sys::ComponentControllerPtr context_provider_controller_;
};

// A basic integration test ensuring a basic cast request launches the right
// URL in the Chromium service.
TEST_F(CastRunnerIntegrationTest, BasicRequest) {
  GURL app_url = test_server_.GetURL(kBlankAppUrl);
  app_config_manager_.AddApp(kTestAppId, app_url);

  CreateComponentContextAndStartComponent();
  fuchsia::web::NavigationControllerPtr nav_controller;
  cast_component_->frame()->GetNavigationController(
      nav_controller.NewRequest());

  // Ensure the NavigationState has the expected URL.
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::NavigationState> nav_entry(
        run_loop.QuitClosure());
    nav_controller->GetVisibleEntry(
        cr_fuchsia::CallbackToFitFunction(nav_entry.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_TRUE(nav_entry->has_url());
    EXPECT_EQ(nav_entry->url(), app_url.spec());
  }

  EXPECT_FALSE(cast_runner_->is_headless());

  // Verify that the component is torn down when |component_controller| is
  // unbound.
  base::RunLoop run_loop;
  component_state_->set_on_delete(run_loop.QuitClosure());
  component_controller_.Unbind();
  run_loop.Run();
}

TEST_F(CastRunnerIntegrationTest, ApiBindings) {
  app_config_manager_.AddApp(kTestAppId, test_server_.GetURL(kEchoAppPath));

  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(cr_fuchsia::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('echoService');",
      "test"));
  binding_list.emplace_back(std::move(echo_binding));
  api_bindings_.set_bindings(std::move(binding_list));

  CreateComponentContextAndStartComponent();

  fuchsia::web::MessagePortPtr port =
      api_bindings_.RunUntilMessagePortReceived("echoService").Bind();

  fuchsia::web::WebMessage message;
  message.set_data(cr_fuchsia::MemBufferFromString("ping", "ping-msg"));
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
  EXPECT_TRUE(component_state_->api_bindings_has_clients());
}

TEST_F(CastRunnerIntegrationTest, IncorrectCastAppId) {
  const char kIncorrectComponentUrl[] = "cast:99999999";

  CreateComponentContext(kIncorrectComponentUrl);
  StartCastComponent(kIncorrectComponentUrl);

  // Run the loop until the ComponentController is dropped, or a WebComponent is
  // created.
  base::RunLoop run_loop;
  component_controller_.set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  cr_fuchsia::ResultReceiver<WebComponent*> web_component(
      run_loop.QuitClosure());
  cast_runner_->SetWebComponentCreatedCallbackForTest(
      AdaptCallbackForRepeating(web_component.GetReceiveCallback()));
  run_loop.Run();
  EXPECT_FALSE(web_component.has_value());
}

TEST_F(CastRunnerIntegrationTest, UrlRequestRewriteRulesProvider) {
  GURL echo_app_url = test_server_.GetURL(kEchoHeaderPath);
  app_config_manager_.AddApp(kTestAppId, echo_app_url);

  CreateComponentContextAndStartComponent();

  // Bind a TestNavigationListener to the Frame.
  cr_fuchsia::TestNavigationListener navigation_listener;
  fidl::Binding<fuchsia::web::NavigationEventListener>
      navigation_listener_binding(&navigation_listener);
  cast_component_->frame()->SetNavigationEventListener(
      navigation_listener_binding.NewBinding());
  navigation_listener.RunUntilUrlEquals(echo_app_url);

  // Check the header was properly set.
  base::Optional<base::Value> result = cr_fuchsia::ExecuteJavaScript(
      cast_component_->frame(), "document.body.innerText");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(result->GetString(), "Value");
}

TEST_F(CastRunnerIntegrationTest, ApplicationControllerBound) {
  app_config_manager_.AddApp(kTestAppId, test_server_.GetURL(kBlankAppUrl));

  CreateComponentContextAndStartComponent();

  // Spin the message loop to handle creation of the component state.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(component_state_);
  EXPECT_TRUE(component_state_->application_context()->controller());
}

// Verify an App launched with remote debugging enabled is properly reachable.
TEST_F(CastRunnerIntegrationTest, RemoteDebugging) {
  GURL app_url = test_server_.GetURL(kBlankAppUrl);
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);
  app_config.set_enable_remote_debugging(true);
  app_config_manager_.AddAppConfig(std::move(app_config));

  CreateComponentContextAndStartComponent();

  // Get the remote debugging port from the Context.
  uint16_t remote_debugging_port = 0;
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<
        fuchsia::web::Context_GetRemoteDebuggingPort_Result>
        port_receiver(run_loop.QuitClosure());
    cast_runner_->GetContext()->GetRemoteDebuggingPort(
        cr_fuchsia::CallbackToFitFunction(port_receiver.GetReceiveCallback()));
    run_loop.Run();

    ASSERT_TRUE(port_receiver->is_response());
    remote_debugging_port = port_receiver->response().port;
    ASSERT_TRUE(remote_debugging_port != 0);
  }

  // Connect to the debug service and ensure we get the proper response.
  base::Value devtools_list =
      cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetList().size(), 1u);

  base::Value* devtools_url = devtools_list.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), app_url.spec());
}

TEST_F(CastRunnerIntegrationTest, IsolatedContext) {
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/echo.html");

  EXPECT_EQ(cast_runner_->GetChildCastRunnerCountForTest(), 0u);

  RegisterAppWithTestData(kContentDirectoryUrl);

  CreateComponentContextAndStartComponent();
  EXPECT_EQ(cast_runner_->GetChildCastRunnerCountForTest(), 1u);

  WaitUrlAndTitle(kContentDirectoryUrl, "echo");

  // Verify that the component is torn down when |component_controller| is
  // unbound.
  base::RunLoop run_loop;
  component_state_->set_on_delete(run_loop.QuitClosure());
  component_controller_.Unbind();
  run_loop.Run();

  EXPECT_EQ(cast_runner_->GetChildCastRunnerCountForTest(), 0u);
}

// Test the lack of CastAgent service does not cause a CastRunner crash.
TEST_F(CastRunnerIntegrationTest, NoCastAgent) {
  app_config_manager_.AddApp(kTestAppId, test_server_.GetURL(kEchoHeaderPath));

  StartCastComponent(base::StringPrintf("cast:%s", kTestAppId));

  base::RunLoop run_loop;
  component_controller_.set_error_handler([&run_loop](zx_status_t error) {
    EXPECT_EQ(error, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  run_loop.Run();
}

// Test the CastAgent disconnecting does not cause a CastRunner crash.
TEST_F(CastRunnerIntegrationTest, DisconnectedCastAgent) {
  app_config_manager_.AddApp(kTestAppId, test_server_.GetURL(kEchoHeaderPath));

  CreateComponentContextAndStartComponent();
  fuchsia::web::NavigationControllerPtr nav_controller;
  cast_component_->frame()->GetNavigationController(
      nav_controller.NewRequest());

  base::RunLoop run_loop;
  component_controller_.set_error_handler([&run_loop](zx_status_t error) {
    EXPECT_EQ(error, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });

  // Tear down the ComponentState, this should close the Agent connection and
  // shut down the CastComponent.
  component_state_->Disconnect();

  run_loop.Run();
}

// Test that the ApiBindings and RewriteRules are received from the secondary
// DummyAgent. This validates that the |agent_url| retrieved from
// AppConfigManager is the one used to retrieve the bindings and the rewrite
// rules.
TEST_F(CastRunnerIntegrationTest, ApplicationConfigAgentUrl) {
  // These are part of the secondary agent, and CastRunner will contact
  // the secondary agent for both of them.
  FakeUrlRequestRewriteRulesProvider dummy_url_request_rewrite_rules_provider;
  TestApiBindings dummy_agent_api_bindings;

  // Indicate that this app is to get bindings from a secondary agent.
  auto app_config = FakeApplicationConfigManager::CreateConfig(
      kTestAppId, test_server_.GetURL(kEchoAppPath));
  app_config.set_agent_url(kDummyAgentUrl);
  app_config_manager_.AddAppConfig(std::move(app_config));

  // Instantiate the bindings that are returned in the multi-agent scenario. The
  // bindings returned for the single-agent scenario are not initialized.
  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(cr_fuchsia::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('dummyService');",
      "test"));
  binding_list.emplace_back(std::move(echo_binding));
  // Assign the bindings to the multi-agent binding.
  dummy_agent_api_bindings.set_bindings(std::move(binding_list));

  auto component_url = base::StringPrintf("cast:%s", kTestAppId);
  CreateComponentContext(component_url);
  EXPECT_NE(component_context_, nullptr);
  component_context_->RegisterCreateComponentStateCallback(
      kDummyAgentUrl,
      base::BindLambdaForTesting(
          [&](base::StringPiece component_url)
              -> std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> {
            return std::make_unique<FakeComponentState>(
                component_url, &app_config_manager_, &dummy_agent_api_bindings,
                &dummy_url_request_rewrite_rules_provider);
          }));

  StartCastComponent(component_url);

  base::RunLoop().RunUntilIdle();

  // Validate that the correct bindings were requested.
  EXPECT_FALSE(component_state_->api_bindings_has_clients());
  // Validate that the correct rewrite rules were requested.
  EXPECT_FALSE(component_state_->url_request_rules_provider_has_clients());
}

// Test that when RewriteRules are not provided, a WebComponent is still
// created. Further validate that the primary agent does not provide ApiBindings
// or RewriteRules.
TEST_F(CastRunnerIntegrationTest, ApplicationConfigAgentUrlRewriteOptional) {
  TestApiBindings dummy_agent_api_bindings;

  // Indicate that this app is to get bindings from a secondary agent.
  auto app_config = FakeApplicationConfigManager::CreateConfig(
      kTestAppId, test_server_.GetURL(kEchoAppPath));
  app_config.set_agent_url(kDummyAgentUrl);
  app_config_manager_.AddAppConfig(std::move(app_config));

  // Instantiate the bindings that are returned in the multi-agent scenario. The
  // bindings returned for the single-agent scenario are not initialized.
  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(cr_fuchsia::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('dummyService');",
      "test"));
  binding_list.emplace_back(std::move(echo_binding));
  // Assign the bindings to the multi-agent binding.
  dummy_agent_api_bindings.set_bindings(std::move(binding_list));

  auto component_url = base::StringPrintf("cast:%s", kTestAppId);
  CreateComponentContext(component_url);
  EXPECT_NE(component_context_, nullptr);
  component_context_->RegisterCreateComponentStateCallback(
      kDummyAgentUrl,
      base::BindLambdaForTesting(
          [&](base::StringPiece component_url)
              -> std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> {
            return std::make_unique<FakeComponentState>(
                component_url, &app_config_manager_, &dummy_agent_api_bindings,
                nullptr);
          }));

  StartCastComponent(component_url);
  WaitComponentCreated();

  base::RunLoop().RunUntilIdle();

  // Validate that the primary agent didn't provide API bindings.
  EXPECT_FALSE(component_state_->api_bindings_has_clients());
  // Validate that the primary agent didn't provide its RewriteRules.
  EXPECT_FALSE(component_state_->url_request_rules_provider_has_clients());
}

TEST_F(CastRunnerIntegrationTest, MicRedirect) {
  GURL app_url = test_server_.GetURL("/mic.html");
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);

  fuchsia::web::PermissionDescriptor mic_permission;
  mic_permission.set_type(fuchsia::web::PermissionType::MICROPHONE);
  app_config.mutable_permissions()->push_back(std::move(mic_permission));
  app_config_manager_.AddAppConfig(std::move(app_config));

  base::RunLoop run_loop;

  init_component_state_callback_ = base::BindOnce(
      [](base::OnceClosure quit_closure, FakeComponentState* component_state) {
        component_state->outgoing_directory()->AddPublicService(
            std::make_unique<vfs::Service>(
                [quit_closure = std::move(quit_closure)](
                    zx::channel channel,
                    async_dispatcher_t* dispatcher) mutable {
                  std::move(quit_closure).Run();
                }),
            fuchsia::media::Audio::Name_);
      },
      base::Passed(run_loop.QuitClosure()));

  CreateComponentContextAndStartComponent();

  run_loop.Run();
}

class HeadlessCastRunnerIntegrationTest : public CastRunnerIntegrationTest {
 public:
  HeadlessCastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(fuchsia::web::ContextFeatureFlags::HEADLESS |
                                  fuchsia::web::ContextFeatureFlags::NETWORK) {}
};

// A basic integration test ensuring a basic cast request launches the right
// URL in the Chromium service.
TEST_F(HeadlessCastRunnerIntegrationTest, Headless) {
  ASSERT_TRUE(cast_runner_->is_headless());

  const char kAnimationPath[] = "/css_animation.html";
  const GURL animation_url = test_server_.GetURL(kAnimationPath);
  app_config_manager_.AddApp(kTestAppId, animation_url);

  CreateComponentContextAndStartComponent();
  auto tokens = scenic::ViewTokenPair::New();
  cast_component_->CreateView(std::move(tokens.view_holder_token.value), {},
                              {});

  WaitUrlAndTitle(animation_url, "animation finished");

  // Verify that dropping the "view" EventPair is handled by the CastComponent.
  {
    base::RunLoop run_loop;
    cast_component_->set_on_headless_disconnect_for_test(
        run_loop.QuitClosure());
    tokens.view_token.value.reset();
    run_loop.Run();
  }

  component_controller_.Unbind();
  base::RunLoop().RunUntilIdle();
}

// Isolated *and* headless? Doesn't sound like much fun!
TEST_F(HeadlessCastRunnerIntegrationTest, IsolatedAndHeadless) {
  ASSERT_TRUE(cast_runner_->is_headless());

  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/echo.html");

  EXPECT_EQ(cast_runner_->GetChildCastRunnerCountForTest(), 0u);

  RegisterAppWithTestData(kContentDirectoryUrl);

  CreateComponentContextAndStartComponent();
  EXPECT_TRUE(cast_component_->runner()->is_headless());
  EXPECT_EQ(cast_runner_->GetChildCastRunnerCountForTest(), 1u);

  WaitUrlAndTitle(kContentDirectoryUrl, "echo");

  // Verify that the component is torn down when |component_controller| is
  // unbound.
  base::RunLoop run_loop;
  component_state_->set_on_delete(run_loop.QuitClosure());
  component_controller_.Unbind();
  run_loop.Run();

  EXPECT_EQ(cast_runner_->GetChildCastRunnerCountForTest(), 0u);
}
