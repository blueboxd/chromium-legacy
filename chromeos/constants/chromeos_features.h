// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace chromeos {
namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file. If a feature is
// being rolled out via Finch, add a comment in the .cc file.

COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAllowScrollSettings;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAmbientModeFeature;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kArcAdbSideloadingFeature;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kArcManagedAdbSideloadingSupport;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAutoScreenBrightness;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAssistAutoCorrect;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAssistPersonalInfo;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAvatarToolbarButton;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothAggressiveAppearanceFilter;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothFixA2dpPacketSize;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothPhoneFilter;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothKernelSuspendNotifier;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothNextHandsfreeProfile;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCameraSystemWebApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniPortForwarding;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniDiskResizing;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniUseBusterImage;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniGpuSupport;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniUsbAllowUnsupported;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniUsername;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniShowMicSetting;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kDisableCryptAuthV1DeviceSync;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceActivityStatus;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniWebUIUpgrader;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceSync;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCryptAuthV2Enrollment;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kDisableOfficeEditingComponentApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kDiscoverApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kDriveFs;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kDlcSettingsUi;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kDriveFsMirroring;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEmojiSuggestAddition;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEolWarningNotifications;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEduCoexistence;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEduCoexistenceConsentLog;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kExoPointerLock;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFilesNG;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFilesZipNoNaCl;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kMojoDBusRelay;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEnableSupervisionTransitionScreens;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFsNosymfollow;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kGaiaActionButtons;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kGesturePropertiesDBusService;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kHelpAppV2;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicHmm;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicFst;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicMozc;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeDecoderWithSandbox;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVirtualKeyboardFloatingDefault;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kInstantTethering;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kLacrosComponentUpdater;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kLacrosSupport;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kMediaApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kNativeRuleBasedTyping;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kNewOsSettingsSearch;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kOobeScreensPriority;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kParentalControlsSettings;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kPluginVmShowCameraSetting;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kPrintJobManagementApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kPrinterStatus;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kQuickAnswers;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickAnswersDogfood;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickAnswersRichUi;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kRar2Fs;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kReleaseNotes;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kReleaseNotesNotification;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kScanningUI;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSessionManagerLongKillTimeout;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShelfHotseat;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowBluetoothDebugLogToggle;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowBluetoothDeviceBattery;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowPlayInDemoMode;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowStepsInDemoModeSetup;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSmartDimExperimentalComponent;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSmartDimNewMlAgent;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSmartDimModelV3;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSplitSettingsSync;
// Visible for testing. Use IsSplitSyncConsentEnabled() to check the flag.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSplitSyncConsent;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSuggestedContentToggle;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUnifiedMediaView;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUpdatedCellularActivationUi;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseMessagesStagingUrl;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUserActivityPrediction;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseSearchClickForRightClick;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVideoPlayerNativeControls;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVirtualKeyboardBorderedKey;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVirtualKeyboardFloatingResizable;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeMozcProto;

// Keep alphabetized.

COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAmbientModeEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAssistantEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsEduCoexistenceEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsImeDecoderWithSandboxEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsInstantTetheringBackgroundAdvertisingSupported();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsLacrosComponentUpdaterEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsLacrosSupportEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsOobeScreensPriorityEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsParentalControlsSettingsEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsQuickAnswersDogfood();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsQuickAnswersEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsQuickAnswersRichUiEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsQuickAnswersSettingToggleEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsSplitSettingsSyncEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsSplitSyncConsentEnabled();

// TODO(michaelpg): Remove after M71 branch to re-enable Play Store by default.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldShowPlayStoreInDemoMode();

COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldUseV1DeviceSync();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldUseV2DeviceSync();

// Keep alphabetized.

}  // namespace features
}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
