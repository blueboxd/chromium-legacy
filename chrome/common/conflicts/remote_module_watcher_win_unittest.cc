// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/conflicts/remote_module_watcher_win.h"

#include <windows.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/conflicts/module_event_sink_win.mojom.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class RemoteModuleWatcherTest : public testing::Test,
                                public service_manager::Service,
                                public mojom::ModuleEventSink {
 public:
  RemoteModuleWatcherTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME_AND_NOW),
        service_binding_(this,
                         test_connector_factory_.RegisterInstance(
                             content::mojom::kSystemServiceName)),
        binding_(this) {}

  ~RemoteModuleWatcherTest() override = default;

  // service_manager::Service:
  void OnStart() override {
    registry_.AddInterface(base::BindRepeating(
        &RemoteModuleWatcherTest::BindModuleEventSinkRequest,
        base::Unretained(this)));
  }
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  // mojom::ModuleEventSink:
  void OnModuleEvents(
      const std::vector<uint64_t>& module_load_addresses) override {
    module_event_count_ += module_load_addresses.size();
  }

  // Returns a connector that may be used to connect to a ModuleEventSink
  // implementation.
  service_manager::Connector* GetConnector() {
    return test_connector_factory_.GetDefaultConnector();
  }

  void LoadModule() {
    if (module_handle_)
      return;
    // This module should not be a static dependency of the unit-test
    // executable, but should be a build-system dependency or a module that is
    // present on any Windows machine.
    static constexpr wchar_t kModuleName[] = L"conflicts_dll.dll";
    // The module should not already be loaded.
    ASSERT_FALSE(::GetModuleHandle(kModuleName));
    // It should load successfully.
    module_handle_ = ::LoadLibrary(kModuleName);
    ASSERT_TRUE(module_handle_);
  }

  void UnloadModule() {
    if (!module_handle_)
      return;
    ::FreeLibrary(module_handle_);
    module_handle_ = nullptr;
  }

  // Runs the task scheduler until no tasks are running.
  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }
  void FastForwardByIdleDelay() {
    scoped_task_environment_.FastForwardBy(RemoteModuleWatcher::kIdleDelay);
  }

  HMODULE module_handle() { return module_handle_; }

  int module_event_count() { return module_event_count_; }

 private:
  void BindModuleEventSinkRequest(mojom::ModuleEventSinkRequest request) {
    binding_.Bind(std::move(request));
  }

  // Must be first.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  service_manager::TestConnectorFactory test_connector_factory_;

  // Used to implement |service_manager::Service|.
  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;

  // Binds a ModuleEventSinkRequest to this implementation of ModuleEventSink.
  mojo::Binding<mojom::ModuleEventSink> binding_;

  // Holds a handle to a loaded module.
  HMODULE module_handle_ = nullptr;

  // Total number of module events seen.
  int module_event_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(RemoteModuleWatcherTest);
};

}  // namespace

TEST_F(RemoteModuleWatcherTest, ModuleEvents) {
  auto remote_module_watcher = RemoteModuleWatcher::Create(
      base::ThreadTaskRunnerHandle::Get(), GetConnector());

  // Wait until the watcher is initialized and events for already loaded modules
  // are received.
  RunUntilIdle();
  // Now wait for the timer used to batch events to expire.
  FastForwardByIdleDelay();

  EXPECT_GT(module_event_count(), 0);

  // Dynamically load a module and ensure a notification is received for it.
  int previous_module_event_count = module_event_count();
  LoadModule();
  FastForwardByIdleDelay();
  EXPECT_GT(module_event_count(), previous_module_event_count);

  UnloadModule();

  // Destroy the module watcher.
  remote_module_watcher = nullptr;
  RunUntilIdle();

  // Load the module and ensure no notification is received this time.
  previous_module_event_count = module_event_count();
  LoadModule();
  FastForwardByIdleDelay();

  EXPECT_EQ(module_event_count(), previous_module_event_count);

  UnloadModule();
}
