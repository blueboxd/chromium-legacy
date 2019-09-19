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

COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kAccountManager;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAdaptiveOOBE;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAmbientModeFeature;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAutoScreenBrightness;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothAggressiveAppearanceFilter;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothPhoneFilter;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBlueZLongTermKeyBlocklist;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kBlueZLongTermKeyBlocklistParamName[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCameraSystemWebApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kCrostiniBackup;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniGpuSupport;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniUsbAllowUnsupported;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniWebUIInstaller;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCryptAuthV2Enrollment;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kDiscoverApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kDriveFs;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kDriveFsMirroring;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEnableFileManagerFeedbackPanel;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEnableFileManagerFormatDialog;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEnableFileManagerPiexWasm;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFilesNG;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kMojoDBusRelay;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEnableSupervisionTransitionScreens;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFsNosymfollow;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kGaiaActionButtons;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kGesturePropertiesDBusService;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicHmm;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicFst;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicFstNonEnglish;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeDecoderWithSandbox;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kInstantTethering;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kMediaApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kParentalControlsSettings;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kReleaseNotes;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowBluetoothDebugLogToggle;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowBluetoothDeviceBattery;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowPlayInDemoMode;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSmartDimModelV3;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSplitSettings;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUpdatedCellularActivationUi;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseMessagesGoogleComDomain;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseMessagesStagingUrl;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUserActivityPrediction;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseSearchClickForRightClick;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVideoPlayerNativeControls;

// Keep alphabetized.

COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAccountManagerEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAdaptiveOobeEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAmbientModeEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAssistantEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsImeDecoderWithSandboxEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsInstantTetheringBackgroundAdvertisingSupported();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsParentalControlsSettingsEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsSplitSettingsEnabled();

// TODO(michaelpg): Remove after M71 branch to re-enable Play Store by default.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldShowPlayStoreInDemoMode();

// Keep alphabetized.

}  // namespace features
}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
