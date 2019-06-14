// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/display/display_configuration_controller_test_api.h"
#include "ash/display/screen_ash.h"
#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/keyboard/test_keyboard_ui.h"
#include "ash/mojo_test_interface_factory.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/test/test_keyboard_controller_observer.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/test_pref_service_provider.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/test/ash_test_views_delegate.h"
#include "ash/test_shell_delegate.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/token.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/audio/cras_audio_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/discardable_memory/public/interfaces/discardable_shared_memory_manager.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/platform_window_defaults.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/wm_state.h"

namespace ash {

AshTestHelper::AshTestHelper()
    : command_line_(std::make_unique<base::test::ScopedCommandLine>()) {
  ui::test::EnableTestConfigForPlatformWindows();
}

AshTestHelper::~AshTestHelper() {
  // Ensure the next test starts with a null display::Screen. Done here because
  // some tests use Screen after TearDown().
  ScreenAsh::DeleteScreenForShutdown();
}

void AshTestHelper::SetUp(bool start_session, bool provide_local_state) {
  // TODO(jamescook): Can we do this without changing command line?
  // Use the origin (1,1) so that it doesn't over
  // lap with the native mouse cursor.
  if (!command_line_->GetProcessCommandLine()->HasSwitch(
          ::switches::kHostWindowBounds)) {
    command_line_->GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kHostWindowBounds, "1+1-800x600");
  }

  statistics_provider_ =
      std::make_unique<chromeos::system::ScopedFakeStatisticsProvider>();

  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      base::BindRepeating(&aura::test::EventGeneratorDelegateAura::Create));

  display::ResetDisplayIdForTest();
  wm_state_ = std::make_unique<::wm::WMState>();
  // Only create a ViewsDelegate if the test didn't create one already.
  if (!views::ViewsDelegate::GetInstance())
    test_views_delegate_ = std::make_unique<AshTestViewsDelegate>();

  // Disable animations during tests.
  zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
  ui::InitializeInputMethodForTesting();

  // Creates Shell and hook with Desktop.
  if (!test_shell_delegate_)
    test_shell_delegate_ = new TestShellDelegate;

  if (!bluez::BluezDBusManager::IsInitialized()) {
    bluez::BluezDBusManager::InitializeFake();
    bluez_dbus_manager_initialized_ = true;
  }

  if (!chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::InitializeFake();

  if (!chromeos::PowerPolicyController::IsInitialized()) {
    chromeos::PowerPolicyController::Initialize(
        chromeos::PowerManagerClient::Get());
    power_policy_controller_initialized_ = true;
  }

  chromeos::CrasAudioClient::InitializeFake();
  // Create CrasAudioHandler for testing since g_browser_process is not
  // created in AshTestBase tests.
  chromeos::CrasAudioHandler::InitializeForTesting();

  // Reset the global state for the cursor manager. This includes the
  // last cursor visibility state, etc.
  ::wm::CursorManager::ResetCursorVisibilityStateForTest();

  ui::MaterialDesignController::Initialize();

  CreateShell(provide_local_state);

  // Reset aura::Env to eliminate test dependency (https://crbug.com/586514).
  aura::test::EnvTestHelper env_helper(aura::Env::GetInstance());
  env_helper.ResetEnvForTesting();

  env_helper.SetInputStateLookup(std::unique_ptr<aura::InputStateLookup>());

  Shell* shell = Shell::Get();

  // Cursor is visible by default in tests.
  // CursorManager is null on MASH.
  if (shell->cursor_manager())
    shell->cursor_manager()->ShowCursor();

  prefs_provider_ = std::make_unique<TestPrefServiceProvider>();
  session_controller_client_.reset(new TestSessionControllerClient(
      shell->session_controller(), prefs_provider_.get()));
  session_controller_client_->InitializeAndSetClient();

  assistant_service_ = std::make_unique<TestAssistantService>();
  shell->assistant_controller()->SetAssistant(
      assistant_service_->CreateInterfacePtrAndBind());

  system_tray_client_ = std::make_unique<TestSystemTrayClient>();
  shell->system_tray_model()->SetClient(system_tray_client_.get());

  if (start_session)
    session_controller_client_->CreatePredefinedUserSessions(1);

  // Tests that change the display configuration generally don't care about
  // the notifications and the popup UI can interfere with things like
  // cursors.
  shell->screen_layout_observer()->set_show_notifications_for_testing(false);

  display::test::DisplayManagerTestApi(shell->display_manager())
      .DisableChangeDisplayUponHostResize();
  DisplayConfigurationControllerTestApi(
      shell->display_configuration_controller())
      .SetDisplayAnimator(false);

  app_list_test_helper_ = std::make_unique<AppListTestHelper>();

  // Create the test keyboard controller observer to respond to
  // OnLoadKeyboardContentsRequested().
  test_keyboard_controller_observer_ =
      std::make_unique<TestKeyboardControllerObserver>(
          shell->ash_keyboard_controller());

  new_window_delegate_ = std::make_unique<TestNewWindowDelegate>();

  // Remove the app dragging animations delay for testing purposes.
  shell->overview_controller()->set_delayed_animation_task_delay_for_test(
      base::TimeDelta());

  // Ensure tests have a wallpaper as placeholder.
  shell->wallpaper_controller()->CreateEmptyWallpaperForTesting();

  TabletModeController::SetForceNoScreenshotForTest();
}

void AshTestHelper::TearDown() {
  app_list_test_helper_.reset();

  Shell::DeleteInstance();
  new_window_delegate_.reset();

  // Needs to be reset after Shell::Get()->ash_keyboard_controller() is deleted.
  test_keyboard_controller_observer_.reset();

  // Suspend the tear down until all resources are returned via
  // CompositorFrameSinkClient::ReclaimResources()
  base::RunLoop().RunUntilIdle();

  chromeos::CrasAudioHandler::Shutdown();
  chromeos::CrasAudioClient::Shutdown();

  if (power_policy_controller_initialized_) {
    chromeos::PowerPolicyController::Shutdown();
    power_policy_controller_initialized_ = false;
  }

  chromeos::PowerManagerClient::Shutdown();

  if (bluez_dbus_manager_initialized_) {
    device::BluetoothAdapterFactory::Shutdown();
    bluez::BluezDBusManager::Shutdown();
    bluez_dbus_manager_initialized_ = false;
  }

  context_factories_.reset();

  // Context factory (and context factory private) referenced by Env are now
  // destroyed. Reset Env's members in case some other test tries to use it.
  // This matters if someone else created Env (such as the test suite) and is
  // long lived.
  if (aura::Env::HasInstance()) {
    aura::Env::GetInstance()->set_context_factory(nullptr);
    aura::Env::GetInstance()->set_context_factory_private(nullptr);
  }

  ui::ShutdownInputMethodForTesting();
  zero_duration_mode_.reset();

  test_views_delegate_.reset();
  wm_state_.reset();

  command_line_.reset();

  display::Display::ResetForceDeviceScaleFactorForTesting();

  CHECK(!::wm::CaptureController::Get());

  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      ui::test::EventGeneratorDelegate::FactoryFunction());

  statistics_provider_.reset();
}

PrefService* AshTestHelper::GetLocalStatePrefService() {
  return Shell::Get()->local_state_;
}

aura::Window* AshTestHelper::CurrentContext() {
  aura::Window* root_window = Shell::GetRootWindowForNewWindows();
  if (!root_window)
    root_window = Shell::GetPrimaryRootWindow();
  DCHECK(root_window);
  return root_window;
}

display::Display AshTestHelper::GetSecondaryDisplay() {
  return Shell::Get()->display_manager()->GetSecondaryDisplay();
}

void AshTestHelper::CreateShell(bool provide_local_state) {
  const bool enable_pixel_output = false;
  context_factories_ =
      std::make_unique<ui::TestContextFactories>(enable_pixel_output);
  ShellInitParams init_params;
  init_params.delegate.reset(test_shell_delegate_);
  init_params.context_factory = context_factories_->GetContextFactory();
  init_params.context_factory_private =
      context_factories_->GetContextFactoryPrivate();
  init_params.keyboard_ui_factory = std::make_unique<TestKeyboardUIFactory>();

  if (provide_local_state) {
    auto pref_service = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(pref_service->registry(), true);

    local_state_ = std::move(pref_service);
    init_params.local_state = local_state_.get();
  }

  Shell::CreateInstance(std::move(init_params));
}

}  // namespace ash
