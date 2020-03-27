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
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/test_keyboard_ui.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/test/test_keyboard_controller_observer.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/public/cpp/test/test_photo_controller.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/test_pref_service_provider.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/system/message_center/test_notifier_settings_controller.h"
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
#include "base/system/sys_info.h"
#include "base/token.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/audio/cras_audio_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/discardable_memory/public/mojom/discardable_shared_memory_manager.mojom.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/wm_state.h"

namespace ash {

AshTestHelper::InitParams::InitParams() = default;
AshTestHelper::InitParams::InitParams(InitParams&&) = default;
AshTestHelper::InitParams::~InitParams() = default;

AshTestHelper::AshTestHelper() = default;

AshTestHelper::~AshTestHelper() {
  // Ensure the next test starts with a null display::Screen.  This must be done
  // here instead of in TearDown() since some tests test access to the Screen
  // after the shell shuts down (which they use TearDown() to trigger).
  ScreenAsh::DeleteScreenForShutdown();
}

void AshTestHelper::SetUp(InitParams init_params) {
  // Aura-general setup -------------------------------------------------------

  wm_state_ = std::make_unique<::wm::WMState>();

  if (init_params.config_type != kShell) {
    ui::test::EnableTestConfigForPlatformWindows();
    ui::InitializeInputMethodForTesting();
  }

  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      base::BindRepeating(&aura::test::EventGeneratorDelegateAura::Create));

  if (init_params.config_type == kUnitTest) {
    zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
  }

  if (!init_params.context_factory) {
    context_factories_ = std::make_unique<ui::TestContextFactories>(false);
    init_params.context_factory = context_factories_->GetContextFactory();
  }

  // Reset aura::Env to eliminate test dependency (https://crbug.com/586514).
  aura::test::EnvTestHelper env_helper(aura::Env::GetInstance());
  env_helper.ResetEnvForTesting();
  env_helper.SetInputStateLookup(std::unique_ptr<aura::InputStateLookup>());

  // Ash-specific setup -------------------------------------------------------

  command_line_ = std::make_unique<base::test::ScopedCommandLine>();
  statistics_provider_ =
      std::make_unique<chromeos::system::ScopedFakeStatisticsProvider>();
  prefs_provider_ = std::make_unique<TestPrefServiceProvider>();
  notifier_settings_controller_ =
      std::make_unique<TestNotifierSettingsController>();
  assistant_service_ = std::make_unique<TestAssistantService>();
  system_tray_client_ = std::make_unique<TestSystemTrayClient>();
  photo_controller_ = std::make_unique<TestPhotoController>();

  // TODO(jamescook): Can we do this without changing command line?
  // Use the origin (1,1) so that it doesn't over
  // lap with the native mouse cursor.
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      !command_line_->GetProcessCommandLine()->HasSwitch(
          ::switches::kHostWindowBounds)) {
    // TODO(oshima): Disable native events instead of adding offset.
    command_line_->GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kHostWindowBounds, "10+10-800x600");
  }

  if (init_params.config_type == kUnitTest)
    TabletModeController::SetUseScreenshotForTest(false);

  if (init_params.config_type != kShell)
    display::ResetDisplayIdForTest();

  chromeos::CrasAudioClient::InitializeFake();
  // Create CrasAudioHandler for testing since g_browser_process is not
  // created in AshTestBase tests.
  chromeos::CrasAudioHandler::InitializeForTesting();

  // Reset the global state for the cursor manager. This includes the
  // last cursor visibility state, etc.
  ::wm::CursorManager::ResetCursorVisibilityStateForTest();

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
  if (!NewWindowDelegate::GetInstance())
    new_window_delegate_ = std::make_unique<TestNewWindowDelegate>();
  if (!views::ViewsDelegate::GetInstance())
    test_views_delegate_ = std::make_unique<AshTestViewsDelegate>();

  ShellInitParams shell_init_params;
  shell_init_params.delegate = std::move(init_params.delegate);
  if (!shell_init_params.delegate)
    shell_init_params.delegate = std::make_unique<TestShellDelegate>();
  shell_init_params.context_factory = init_params.context_factory;
  shell_init_params.local_state = init_params.local_state;
  shell_init_params.keyboard_ui_factory =
      std::make_unique<TestKeyboardUIFactory>();
  Shell::CreateInstance(std::move(shell_init_params));
  Shell* shell = Shell::Get();

  // Cursor is visible by default in tests.
  shell->cursor_manager()->ShowCursor();

  shell->assistant_controller()->SetAssistant(
      assistant_service_->CreateRemoteAndBind());

  shell->system_tray_model()->SetClient(system_tray_client_.get());

  session_controller_client_.reset(new TestSessionControllerClient(
      shell->session_controller(), prefs_provider_.get()));
  session_controller_client_->InitializeAndSetClient();
  if (init_params.start_session)
    session_controller_client_->CreatePredefinedUserSessions(1);

  // Requires the AppListController the Shell creates.
  app_list_test_helper_ = std::make_unique<AppListTestHelper>();

  if (init_params.config_type == kShell) {
    shell->wallpaper_controller()->ShowDefaultWallpaperForTesting();
    return;
  }

  // Don't change the display size due to host size resize.
  display::test::DisplayManagerTestApi(shell->display_manager())
      .DisableChangeDisplayUponHostResize();

  // Create the test keyboard controller observer to respond to
  // OnLoadKeyboardContentsRequested().
  test_keyboard_controller_observer_ =
      std::make_unique<TestKeyboardControllerObserver>(
          shell->keyboard_controller());

  if (init_params.config_type != kUnitTest)
    return;

  // Tests that change the display configuration generally don't care about the
  // notifications and the popup UI can interfere with things like cursors.
  shell->screen_layout_observer()->set_show_notifications_for_testing(false);

  // Disable display change animations in unit tests.
  DisplayConfigurationControllerTestApi(
      shell->display_configuration_controller())
      .SetDisplayAnimator(false);

  // Remove the app dragging animations delay for testing purposes.
  shell->overview_controller()->set_delayed_animation_task_delay_for_test(
      base::TimeDelta());

  // Tests expect empty wallpaper.
  shell->wallpaper_controller()->CreateEmptyWallpaperForTesting();
}

void AshTestHelper::TearDown() {
  // Ash-specific teardown ----------------------------------------------------

  // The AppListTestHelper holds a pointer to the AppListController the Shell
  // owns, so shut the test helper down first.
  app_list_test_helper_.reset();

  Shell::DeleteInstance();

  // Suspend the tear down until all resources are returned via
  // CompositorFrameSinkClient::ReclaimResources()
  base::RunLoop().RunUntilIdle();

  chromeos::CrasAudioHandler::Shutdown();
  chromeos::CrasAudioClient::Shutdown();

  // The PowerPolicyController holds a pointer to the PowerManagementClient, so
  // shut the controller down first.
  if (power_policy_controller_initialized_) {
    chromeos::PowerPolicyController::Shutdown();
    power_policy_controller_initialized_ = false;
  }

  chromeos::PowerManagerClient::Shutdown();

  TabletModeController::SetUseScreenshotForTest(true);

  // Destroy all owned objects to prevent tests from depending on their state
  // after this returns.
  test_keyboard_controller_observer_.reset();
  test_views_delegate_.reset();
  new_window_delegate_.reset();
  if (bluez_dbus_manager_initialized_) {
    device::BluetoothAdapterFactory::Shutdown();
    bluez::BluezDBusManager::Shutdown();
    bluez_dbus_manager_initialized_ = false;
  }
  photo_controller_.reset();
  system_tray_client_.reset();
  assistant_service_.reset();
  notifier_settings_controller_.reset();
  prefs_provider_.reset();
  statistics_provider_.reset();
  command_line_.reset();

  // Aura-general teardown ----------------------------------------------------

  ui::ShutdownInputMethodForTesting();

  // Context factory referenced by Env is now destroyed. Reset Env's members in
  // case some other test tries to use it. This matters if someone else created
  // Env (such as the test suite) and is long lived.
  if (aura::Env::HasInstance())
    aura::Env::GetInstance()->set_context_factory(nullptr);

  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      ui::test::EventGeneratorDelegate::FactoryFunction());

  context_factories_.reset();
  zero_duration_mode_.reset();
  wm_state_.reset();
}

PrefService* AshTestHelper::GetLocalStatePrefService() {
  return Shell::Get()->local_state_;
}

aura::Window* AshTestHelper::GetContext() {
  aura::Window* root_window = Shell::GetRootWindowForNewWindows();
  if (!root_window)
    root_window = Shell::GetPrimaryRootWindow();
  DCHECK(root_window);
  return root_window;
}

display::Display AshTestHelper::GetSecondaryDisplay() const {
  return display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .GetSecondaryDisplay();
}

}  // namespace ash
