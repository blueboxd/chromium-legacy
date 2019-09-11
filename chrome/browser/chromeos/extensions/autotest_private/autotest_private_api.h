// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/window_state_type.h"
#include "base/compiler_specific.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/snapshot/screenshot_grabber.h"

namespace crostini {
enum class CrostiniResult;
}

namespace extensions {

class AutotestPrivateLogoutFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.logout", AUTOTESTPRIVATE_LOGOUT)

 private:
  ~AutotestPrivateLogoutFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateRestartFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.restart", AUTOTESTPRIVATE_RESTART)

 private:
  ~AutotestPrivateRestartFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateShutdownFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.shutdown",
                             AUTOTESTPRIVATE_SHUTDOWN)

 private:
  ~AutotestPrivateShutdownFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLoginStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.loginStatus",
                             AUTOTESTPRIVATE_LOGINSTATUS)

 private:
  ~AutotestPrivateLoginStatusFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLockScreenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.lockScreen",
                             AUTOTESTPRIVATE_LOCKSCREEN)

 private:
  ~AutotestPrivateLockScreenFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetExtensionsInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getExtensionsInfo",
                             AUTOTESTPRIVATE_GETEXTENSIONSINFO)

 private:
  ~AutotestPrivateGetExtensionsInfoFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSimulateAsanMemoryBugFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.simulateAsanMemoryBug",
                             AUTOTESTPRIVATE_SIMULATEASANMEMORYBUG)

 private:
  ~AutotestPrivateSimulateAsanMemoryBugFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetTouchpadSensitivityFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setTouchpadSensitivity",
                             AUTOTESTPRIVATE_SETTOUCHPADSENSITIVITY)

 private:
  ~AutotestPrivateSetTouchpadSensitivityFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetTapToClickFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setTapToClick",
                             AUTOTESTPRIVATE_SETTAPTOCLICK)

 private:
  ~AutotestPrivateSetTapToClickFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetThreeFingerClickFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setThreeFingerClick",
                             AUTOTESTPRIVATE_SETTHREEFINGERCLICK)

 private:
  ~AutotestPrivateSetThreeFingerClickFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetTapDraggingFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setTapDragging",
                             AUTOTESTPRIVATE_SETTAPDRAGGING)

 private:
  ~AutotestPrivateSetTapDraggingFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetNaturalScrollFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setNaturalScroll",
                             AUTOTESTPRIVATE_SETNATURALSCROLL)

 private:
  ~AutotestPrivateSetNaturalScrollFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetMouseSensitivityFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setMouseSensitivity",
                             AUTOTESTPRIVATE_SETMOUSESENSITIVITY)

 private:
  ~AutotestPrivateSetMouseSensitivityFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetPrimaryButtonRightFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setPrimaryButtonRight",
                             AUTOTESTPRIVATE_SETPRIMARYBUTTONRIGHT)

 private:
  ~AutotestPrivateSetPrimaryButtonRightFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetMouseReverseScrollFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setMouseReverseScroll",
                             AUTOTESTPRIVATE_SETMOUSEREVERSESCROLL)

 private:
  ~AutotestPrivateSetMouseReverseScrollFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetVisibleNotificationsFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateGetVisibleNotificationsFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getVisibleNotifications",
                             AUTOTESTPRIVATE_GETVISIBLENOTIFICATIONS)

 private:
  ~AutotestPrivateGetVisibleNotificationsFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetPlayStoreStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getPlayStoreState",
                             AUTOTESTPRIVATE_GETPLAYSTORESTATE)

 private:
  ~AutotestPrivateGetPlayStoreStateFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetArcStartTimeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcStartTime",
                             AUTOTESTPRIVATE_GETARCSTARTTIME)

 private:
  ~AutotestPrivateGetArcStartTimeFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetArcStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcState",
                             AUTOTESTPRIVATE_GETARCSTATE)

 private:
  ~AutotestPrivateGetArcStateFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetPlayStoreEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setPlayStoreEnabled",
                             AUTOTESTPRIVATE_SETPLAYSTOREENABLED)

 private:
  ~AutotestPrivateSetPlayStoreEnabledFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetHistogramFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getHistogram",
                             AUTOTESTPRIVATE_GETHISTOGRAM)

 private:
  ~AutotestPrivateGetHistogramFunction() override;
  ResponseAction Run() override;

  // Sends an asynchronous response containing data for the histogram named
  // |name|. Passed to content::FetchHistogramsAsynchronously() to be run after
  // new data from other processes has been collected.
  void RespondOnHistogramsFetched(const std::string& name);

  // Creates a response with current data for the histogram named |name|.
  ResponseValue GetHistogram(const std::string& name);
};

class AutotestPrivateIsAppShownFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.isAppShown",
                             AUTOTESTPRIVATE_ISAPPSHOWN)

 private:
  ~AutotestPrivateIsAppShownFunction() override;
  ResponseAction Run() override;
};

// Deprecated, use GetArcState instead.
class AutotestPrivateIsArcProvisionedFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.isArcProvisioned",
                             AUTOTESTPRIVATE_ISARCPROVISIONED)

 private:
  ~AutotestPrivateIsArcProvisionedFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetArcAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcApp",
                             AUTOTESTPRIVATE_GETARCAPP)

 private:
  ~AutotestPrivateGetArcAppFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetArcPackageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcPackage",
                             AUTOTESTPRIVATE_GETARCPACKAGE)

 private:
  ~AutotestPrivateGetArcPackageFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLaunchArcAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.launchArcApp",
                             AUTOTESTPRIVATE_LAUNCHARCAPP)

 private:
  ~AutotestPrivateLaunchArcAppFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLaunchAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.launchApp",
                             AUTOTESTPRIVATE_LAUNCHAPP)

 private:
  ~AutotestPrivateLaunchAppFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateCloseAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.closeApp",
                             AUTOTESTPRIVATE_CLOSEAPP)

 private:
  ~AutotestPrivateCloseAppFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetCrostiniEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setCrostiniEnabled",
                             AUTOTESTPRIVATE_SETCROSTINIENABLED)

 private:
  ~AutotestPrivateSetCrostiniEnabledFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateRunCrostiniInstallerFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.runCrostiniInstaller",
                             AUTOTESTPRIVATE_RUNCROSTINIINSTALLER)

 private:
  ~AutotestPrivateRunCrostiniInstallerFunction() override;
  ResponseAction Run() override;

  void CrostiniRestarted(crostini::CrostiniResult);
};

class AutotestPrivateRunCrostiniUninstallerFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.runCrostiniUninstaller",
                             AUTOTESTPRIVATE_RUNCROSTINIUNINSTALLER)

 private:
  ~AutotestPrivateRunCrostiniUninstallerFunction() override;
  ResponseAction Run() override;

  void CrostiniRemoved(crostini::CrostiniResult);
};

class AutotestPrivateExportCrostiniFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.exportCrostini",
                             AUTOTESTPRIVATE_EXPORTCROSTINI)

 private:
  ~AutotestPrivateExportCrostiniFunction() override;
  ResponseAction Run() override;

  void CrostiniExported(crostini::CrostiniResult);
};

class AutotestPrivateImportCrostiniFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.importCrostini",
                             AUTOTESTPRIVATE_IMPORTCROSTINI)

 private:
  ~AutotestPrivateImportCrostiniFunction() override;
  ResponseAction Run() override;

  void CrostiniImported(crostini::CrostiniResult);
};

class AutotestPrivateRegisterComponentFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.registerComponent",
                             AUTOTESTPRIVATE_REGISTERCOMPONENT)

 private:
  ~AutotestPrivateRegisterComponentFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateTakeScreenshotFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.takeScreenshot",
                             AUTOTESTPRIVATE_TAKESCREENSHOT)

 private:
  ~AutotestPrivateTakeScreenshotFunction() override;
  ResponseAction Run() override;

  void ScreenshotTaken(std::unique_ptr<ui::ScreenshotGrabber> grabber,
                       ui::ScreenshotResult screenshot_result,
                       scoped_refptr<base::RefCountedMemory> png_data);
};

class AutotestPrivateTakeScreenshotForDisplayFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.takeScreenshotForDisplay",
                             AUTOTESTPRIVATE_TAKESCREENSHOTFORDISPLAY)

 private:
  ~AutotestPrivateTakeScreenshotForDisplayFunction() override;
  ResponseAction Run() override;

  void ScreenshotTaken(std::unique_ptr<ui::ScreenshotGrabber> grabber,
                       ui::ScreenshotResult screenshot_result,
                       scoped_refptr<base::RefCountedMemory> png_data);
};

class AutotestPrivateGetPrinterListFunction
    : public ExtensionFunction,
      public chromeos::CupsPrintersManager::Observer {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getPrinterList",
                             AUTOTESTPRIVATE_GETPRINTERLIST)
  AutotestPrivateGetPrinterListFunction();

 private:
  ~AutotestPrivateGetPrinterListFunction() override;
  ResponseAction Run() override;
  void RespondWithTimeoutError();
  void RespondWithSuccess();
  // chromeos::CupsPrintersManager::Observer
  void OnEnterprisePrintersInitialized() override;
  std::unique_ptr<base::Value> results_;
  std::unique_ptr<chromeos::CupsPrintersManager> printers_manager_;
  base::OneShotTimer timeout_timer_;
};

class AutotestPrivateUpdatePrinterFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.updatePrinter",
                             AUTOTESTPRIVATE_UPDATEPRINTER)

 private:
  ~AutotestPrivateUpdatePrinterFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateRemovePrinterFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.removePrinter",
                             AUTOTESTPRIVATE_REMOVEPRINTER)

 private:
  ~AutotestPrivateRemovePrinterFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetAllEnterprisePoliciesFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getAllEnterprisePolicies",
                             AUTOTESTPRIVATE_GETALLENTERPRISEPOLICIES)

 private:
  ~AutotestPrivateGetAllEnterprisePoliciesFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateBootstrapMachineLearningServiceFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateBootstrapMachineLearningServiceFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.bootstrapMachineLearningService",
                             AUTOTESTPRIVATE_BOOTSTRAPMACHINELEARNINGSERVICE)

 private:
  ~AutotestPrivateBootstrapMachineLearningServiceFunction() override;
  ResponseAction Run() override;

  // Callbacks for a basic Mojo call to MachineLearningService.LoadModel.
  void ModelLoaded(chromeos::machine_learning::mojom::LoadModelResult result);
  void ConnectionError();

  chromeos::machine_learning::mojom::ModelPtr model_;
};

// Enable/disable the Google Assistant feature. This toggles the Assistant user
// pref which will indirectly bring up or shut down the Assistant service.
class AutotestPrivateSetAssistantEnabledFunction
    : public ExtensionFunction,
      public ash::AssistantStateObserver {
 public:
  AutotestPrivateSetAssistantEnabledFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setAssistantEnabled",
                             AUTOTESTPRIVATE_SETASSISTANTENABLED)

 private:
  ~AutotestPrivateSetAssistantEnabledFunction() override;
  ResponseAction Run() override;

  // ash::AssistantStateObserver overrides:
  void OnAssistantStatusChanged(ash::mojom::AssistantState state) override;

  // Called when the Assistant service does not respond in a timely fashion. We
  // will respond with an error.
  void Timeout();

  base::Optional<bool> enabled_;
  base::OneShotTimer timeout_timer_;
};

// Send text query to Assistant and return response.
class AutotestPrivateSendAssistantTextQueryFunction
    : public ExtensionFunction,
      public chromeos::assistant::mojom::AssistantInteractionSubscriber {
 public:
  AutotestPrivateSendAssistantTextQueryFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.sendAssistantTextQuery",
                             AUTOTESTPRIVATE_SENDASSISTANTTEXTQUERY)

 private:
  ~AutotestPrivateSendAssistantTextQueryFunction() override;
  ResponseAction Run() override;

  using AssistantInteractionMetadataPtr =
      chromeos::assistant::mojom::AssistantInteractionMetadataPtr;
  using AssistantInteractionResolution =
      chromeos::assistant::mojom::AssistantInteractionResolution;
  using AssistantSuggestionPtr =
      chromeos::assistant::mojom::AssistantSuggestionPtr;

  // chromeos::assistant::mojom::AssistantInteractionSubscriber:
  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override;
  void OnTextResponse(const std::string& response) override;
  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override;
  void OnInteractionStarted(AssistantInteractionMetadataPtr metadata) override {
  }
  void OnSuggestionsResponse(
      std::vector<AssistantSuggestionPtr> response) override {}
  void OnOpenUrlResponse(const GURL& url, bool in_background) override {}
  void OnSpeechRecognitionStarted() override {}
  void OnSpeechRecognitionIntermediateResult(
      const std::string& high_confidence_text,
      const std::string& low_confidence_text) override {}
  void OnSpeechRecognitionEndOfUtterance() override {}
  void OnSpeechRecognitionFinalResult(
      const std::string& final_result) override {}
  void OnSpeechLevelUpdated(float speech_level) override {}
  void OnTtsStarted(bool due_to_error) override {}
  void OnWaitStarted() override {}

  // Called when Assistant service fails to respond in a certain amount of
  // time. We will respond with an error.
  void Timeout();

  chromeos::assistant::mojom::AssistantPtr assistant_;
  mojo::Receiver<chromeos::assistant::mojom::AssistantInteractionSubscriber>
      assistant_interaction_subscriber_receiver_{this};
  base::OneShotTimer timeout_timer_;
  std::unique_ptr<base::DictionaryValue> result_;
};

// Set user pref value in the pref tree.
class AutotestPrivateSetWhitelistedPrefFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setWhitelistedPref",
                             AUTOTESTPRIVATE_SETWHITELISTEDPREF)

 private:
  ~AutotestPrivateSetWhitelistedPrefFunction() override;
  ResponseAction Run() override;
};

// Enable/disable a Crostini app's "scaled" property.
// When an app is "scaled", it will use low display density.
class AutotestPrivateSetCrostiniAppScaledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setCrostiniAppScaled",
                             AUTOTESTPRIVATE_SETCROSTINIAPPSCALED)
 private:
  ~AutotestPrivateSetCrostiniAppScaledFunction() override;
  ResponseAction Run() override;
};

// The profile-keyed service that manages the autotestPrivate extension API.
class AutotestPrivateAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<AutotestPrivateAPI>*
  GetFactoryInstance();

  // TODO(achuith): Replace these with a mock object for system calls.
  bool test_mode() const { return test_mode_; }
  void set_test_mode(bool test_mode) { test_mode_ = test_mode; }

 private:
  friend class BrowserContextKeyedAPIFactory<AutotestPrivateAPI>;

  AutotestPrivateAPI();
  ~AutotestPrivateAPI() override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "AutotestPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  bool test_mode_;  // true for AutotestPrivateApiTest.AutotestPrivate test.
};

// Get the primary display's scale factor.
class AutotestPrivateGetPrimaryDisplayScaleFactorFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getPrimaryDisplayScaleFactor",
                             AUTOTESTPRIVATE_GETPRIMARYDISPLAYSCALEFACTOR)
 private:
  ~AutotestPrivateGetPrimaryDisplayScaleFactorFunction() override;
  ResponseAction Run() override;
};

// Returns if tablet mode is enabled.
class AutotestPrivateIsTabletModeEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.isTabletModeEnabled",
                             AUTOTESTPRIVATE_ISTABLETMODEENABLED)
 private:
  ~AutotestPrivateIsTabletModeEnabledFunction() override;
  ResponseAction Run() override;
};

// Enables/Disables tablet mode.
class AutotestPrivateSetTabletModeEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setTabletModeEnabled",
                             AUTOTESTPRIVATE_SETTABLETMODEENABLED)

 private:
  ~AutotestPrivateSetTabletModeEnabledFunction() override;
  ResponseAction Run() override;
};

// Returns a list of all installed applications
class AutotestPrivateGetAllInstalledAppsFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetAllInstalledAppsFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getAllInstalledApps",
                             AUTOTESTPRIVATE_GETALLINSTALLEDAPPS)

 private:
  ~AutotestPrivateGetAllInstalledAppsFunction() override;
  ResponseAction Run() override;
};

// Returns a list of all shelf items
class AutotestPrivateGetShelfItemsFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetShelfItemsFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getShelfItems",
                             AUTOTESTPRIVATE_GETSHELFITEMS)

 private:
  ~AutotestPrivateGetShelfItemsFunction() override;
  ResponseAction Run() override;
};

// Returns the shelf auto hide behavior.
class AutotestPrivateGetShelfAutoHideBehaviorFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateGetShelfAutoHideBehaviorFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getShelfAutoHideBehavior",
                             AUTOTESTPRIVATE_GETSHELFAUTOHIDEBEHAVIOR)

 private:
  ~AutotestPrivateGetShelfAutoHideBehaviorFunction() override;
  ResponseAction Run() override;
};

// Sets shelf autohide behavior.
class AutotestPrivateSetShelfAutoHideBehaviorFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateSetShelfAutoHideBehaviorFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setShelfAutoHideBehavior",
                             AUTOTESTPRIVATE_SETSHELFAUTOHIDEBEHAVIOR)

 private:
  ~AutotestPrivateSetShelfAutoHideBehaviorFunction() override;
  ResponseAction Run() override;
};

// Returns the shelf alignment.
class AutotestPrivateGetShelfAlignmentFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetShelfAlignmentFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getShelfAlignment",
                             AUTOTESTPRIVATE_GETSHELFALIGNMENT)

 private:
  ~AutotestPrivateGetShelfAlignmentFunction() override;
  ResponseAction Run() override;
};

// Sets shelf alignment.
class AutotestPrivateSetShelfAlignmentFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetShelfAlignmentFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setShelfAlignment",
                             AUTOTESTPRIVATE_SETSHELFALIGNMENT)

 private:
  ~AutotestPrivateSetShelfAlignmentFunction() override;
  ResponseAction Run() override;
};

// Returns the overview mode state.
class AutotestPrivateSetOverviewModeStateFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetOverviewModeStateFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setOverviewModeState",
                             AUTOTESTPRIVATE_SETOVERVIEWMODESTATE)

 private:
  ~AutotestPrivateSetOverviewModeStateFunction() override;
  ResponseAction Run() override;

  // Called when the overview mode changes.
  void OnOverviewModeChanged(bool for_start, bool finished);
};

class AutotestPrivateShowVirtualKeyboardIfEnabledFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateShowVirtualKeyboardIfEnabledFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.showVirtualKeyboardIfEnabled",
                             AUTOTESTPRIVATE_SHOWVIRTUALKEYBOARDIFENABLED)

 private:
  ~AutotestPrivateShowVirtualKeyboardIfEnabledFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetArcAppWindowStateFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetArcAppWindowStateFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setArcAppWindowState",
                             AUTOTESTPRIVATE_SETARCAPPWINDOWSTATE)

 private:
  class WindowStateChangeObserver;

  ~AutotestPrivateSetArcAppWindowStateFunction() override;
  ResponseAction Run() override;

  // Callback function to be called after window state is changed.
  void WindowStateChanged(ash::WindowStateType expected_type, bool success);

  std::unique_ptr<WindowStateChangeObserver> window_state_observer_;
};

class AutotestPrivateSetArcAppWindowFocusFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetArcAppWindowFocusFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setArcAppWindowFocus",
                             AUTOTESTPRIVATE_SETARCAPPWINDOWFOCUS)

 private:
  ~AutotestPrivateSetArcAppWindowFocusFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetArcAppWindowStateFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetArcAppWindowStateFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcAppWindowState",
                             AUTOTESTPRIVATE_GETARCAPPWINDOWSTATE)

 private:
  ~AutotestPrivateGetArcAppWindowStateFunction() override;
  ResponseAction Run() override;
};

// Gets various window properties of an ARC window.
class AutotestPrivateGetArcAppWindowInfoFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetArcAppWindowInfoFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcAppWindowInfo",
                             AUTOTESTPRIVATE_GETARCAPPWINDOWINFO)

 private:
  ~AutotestPrivateGetArcAppWindowInfoFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSwapWindowsInSplitViewFunction : public ExtensionFunction {
 public:
  AutotestPrivateSwapWindowsInSplitViewFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.swapWindowsInSplitView",
                             AUTOTESTPRIVATE_SWAPWINDOWSINSPLITVIEW)

 private:
  ~AutotestPrivateSwapWindowsInSplitViewFunction() override;
  ResponseAction Run() override;
};

template <>
KeyedService*
BrowserContextKeyedAPIFactory<AutotestPrivateAPI>::BuildServiceInstanceFor(
    content::BrowserContext* context) const;

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_
