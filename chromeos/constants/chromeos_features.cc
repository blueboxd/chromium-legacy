// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

namespace chromeos {
namespace features {
namespace {

// Controls whether Instant Tethering supports hosts which use the background
// advertisement model.
const base::Feature kInstantTetheringBackgroundAdvertisementSupport{
    "InstantTetheringBackgroundAdvertisementSupport",
    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

// Controls whether to enable Chrome OS Account Manager.
// Rollout controlled by Finch.
const base::Feature kAccountManager{"ChromeOSAccountManager",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables adaptive OOBE UX.
const base::Feature kAdaptiveOobe{"AdaptiveOobe",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable Ambient mode feature.
const base::Feature kAmbientModeFeature{"ChromeOSAmbientMode",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables auto screen-brightness adjustment when ambient light
// changes.
const base::Feature kAutoScreenBrightness{"AutoScreenBrightness",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables more aggressive filtering out of Bluetooth devices with
// "appearances" that are less likely to be pairable or useful.
const base::Feature kBluetoothAggressiveAppearanceFilter{
    "BluetoothAggressiveAppearanceFilter", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables more filtering out of phones from the Bluetooth UI.
const base::Feature kBluetoothPhoneFilter{"BluetoothPhoneFilter",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Feature containing param to block provided long term keys.
const base::Feature kBlueZLongTermKeyBlocklist{
    "BlueZLongTermKeyBlocklist", base::FEATURE_DISABLED_BY_DEFAULT};
const char kBlueZLongTermKeyBlocklistParamName[] = "ltk_blocklist";

// Enable or disables running the Camera App as a System Web App.
const base::Feature kCameraSystemWebApp{"CameraSystemWebApp",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini Backup.
const base::Feature kCrostiniBackup{"CrostiniBackup",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini GPU support.
const base::Feature kCrostiniGpuSupport{"CrostiniGpuSupport",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini usb mounting for unsupported devices.
const base::Feature kCrostiniUsbAllowUnsupported{
    "CrostiniUsbAllowUnsupported", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the new WebUI Crostini installer.
const base::Feature kCrostiniWebUIInstaller{"CrostiniWebUIInstaller",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 Enrollment flow.
const base::Feature kCryptAuthV2Enrollment{"CryptAuthV2Enrollment",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Discover Application on Chrome OS.
// If enabled, Discover App will be shown in launcher.
const base::Feature kDiscoverApp{"DiscoverApp",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, DriveFS will be used for Drive sync.
const base::Feature kDriveFs{"DriveFS", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables DriveFS' experimental local files mirroring functionality.
const base::Feature kDriveFsMirroring{"DriveFsMirroring",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled shows the visual signals feedback panel.
const base::Feature kEnableFileManagerFeedbackPanel{
    "EnableFeedbackPanel", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the enhanced external media formatting dialog in the file manager,
// with support for labelling and also NTFS/exFAT filesystems.
const base::Feature kEnableFileManagerFormatDialog{
    "EnableFileManagerFormatDialog", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable the piex-wasm module for raw image preview image extraction.
const base::Feature kEnableFileManagerPiexWasm{
    "PiexWasm", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the next generation file manager.
const base::Feature kFilesNG{"FilesNG", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the use of Mojo by Chrome-process code to communicate with Power
// Manager. In order to use mojo, this feature must be turned on and a callsite
// must use PowerManagerMojoClient::Get().
const base::Feature kMojoDBusRelay{"MojoDBusRelay",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, will display blocking screens during re-authentication after a
// supervision transition occurred.
const base::Feature kEnableSupervisionTransitionScreens{
    "EnableSupervisionTransitionScreens", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable restriction of symlink traversal on user-supplied filesystems.
const base::Feature kFsNosymfollow{"FsNosymfollow",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enable a D-Bus service for accessing gesture properties.
const base::Feature kGesturePropertiesDBusService{
    "GesturePropertiesDBusService", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable primary/secondary action buttons on Gaia login screen.
const base::Feature kGaiaActionButtons{"GaiaActionButtons",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable Unified Input Logic for HMM decoder in the IME extension
// on Chrome OS.
const base::Feature kImeInputLogicHmm{"ImeInputLogicHmm",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable Unified Input Logic for FST decoder in the IME extension
// on Chrome OS.
const base::Feature kImeInputLogicFst{"ImeInputLogicFst",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable Unified Input Logic for FST decoder for non-English in
// the IME extension on Chrome OS.
const base::Feature kImeInputLogicFstNonEnglish{
    "ImeInputLogicFstNonEnglish", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable IME service decoder engine and 'ime' sandbox on Chrome OS.
const base::Feature kImeDecoderWithSandbox{"ImeDecoderWithSandbox",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Instant Tethering on Chrome OS.
const base::Feature kInstantTethering{"InstantTethering",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// ChromeOS Media App. https://crbug.com/996088.
const base::Feature kMediaApp{"MediaApp", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable the Parental Controls section of settings.
const base::Feature kParentalControlsSettings{
    "ChromeOSParentalControlsSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Release Notes on Chrome OS.
const base::Feature kReleaseNotes{"ReleaseNotes",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables a toggle to enable Bluetooth debug logs.
const base::Feature kShowBluetoothDebugLogToggle{
    "ShowBluetoothDebugLogToggle", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables showing the battery level in the System Tray and Settings
// UI for supported Bluetooth Devices.
const base::Feature kShowBluetoothDeviceBattery{
    "ShowBluetoothDeviceBattery", base::FEATURE_DISABLED_BY_DEFAULT};

// Shows the Play Store icon in Demo Mode.
const base::Feature kShowPlayInDemoMode{"ShowPlayInDemoMode",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Uses the V3 (~2019-05 era) Smart Dim model instead of the default V2
// (~2018-11) model.
const base::Feature kSmartDimModelV3{"SmartDimModelV3",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Splits OS settings (display, mouse, keyboard, etc.) out from browser settings
// into a separate window.
const base::Feature kSplitSettings{"SplitSettings",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the updated cellular activation UI; see go/cros-cellular-design.
const base::Feature kUpdatedCellularActivationUi{
    "UpdatedCellularActivationUi", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the messages.google.com domain as part of the "Messages" feature under
// "Connected Devices" settings.
const base::Feature kUseMessagesGoogleComDomain{
    "UseMessagesGoogleComDomain", base::FEATURE_ENABLED_BY_DEFAULT};

// Use the staging URL as part of the "Messages" feature under "Connected
// Devices" settings.
const base::Feature kUseMessagesStagingUrl{"UseMessagesStagingUrl",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables user activity prediction for power management on
// Chrome OS.
// Defined here rather than in //chrome alongside other related features so that
// PowerPolicyController can check it.
const base::Feature kUserActivityPrediction{"UserActivityPrediction",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Remap search+click to right click instead of the legacy alt+click on
// Chrome OS.
const base::Feature kUseSearchClickForRightClick{
    "UseSearchClickForRightClick", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable native controls in video player on Chrome OS.
const base::Feature kVideoPlayerNativeControls{
    "VideoPlayerNativeControls", base::FEATURE_ENABLED_BY_DEFAULT};

////////////////////////////////////////////////////////////////////////////////

bool IsAccountManagerEnabled() {
  return base::FeatureList::IsEnabled(kAccountManager);
}

bool IsAdaptiveOobeEnabled() {
  return base::FeatureList::IsEnabled(kAdaptiveOobe);
}

bool IsAmbientModeEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeFeature);
}

bool IsImeDecoderWithSandboxEnabled() {
  return base::FeatureList::IsEnabled(kImeDecoderWithSandbox);
}

bool IsInstantTetheringBackgroundAdvertisingSupported() {
  return base::FeatureList::IsEnabled(
      kInstantTetheringBackgroundAdvertisementSupport);
}

bool IsParentalControlsSettingsEnabled() {
  return base::FeatureList::IsEnabled(kParentalControlsSettings);
}

bool IsSplitSettingsEnabled() {
  return base::FeatureList::IsEnabled(kSplitSettings);
}

bool ShouldShowPlayStoreInDemoMode() {
  return base::FeatureList::IsEnabled(kShowPlayInDemoMode);
}

}  // namespace features
}  // namespace chromeos
