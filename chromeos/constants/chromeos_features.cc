// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

namespace chromeos {

namespace features {

// Enables or disables auto screen-brightness adjustment when ambient light
// changes.
const base::Feature kAutoScreenBrightness{"AutoScreenBrightness",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Feature containing param to block provided long term keys.
const base::Feature kBlueZLongTermKeyBlocklist{
    "BlueZLongTermKeyBlocklist", base::FEATURE_DISABLED_BY_DEFAULT};
const char kBlueZLongTermKeyBlocklistParamName[] = "ltk_blocklist";

// Enables or disables Crostini Backup.
const base::Feature kCrostiniBackup{"CrostiniBackup",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini GPU support.
const base::Feature kCrostiniGpuSupport{"CrostiniGpuSupport",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini support for usb mounting.
const base::Feature kCrostiniUsbSupport{"CrostiniUsbSupport",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini usb mounting for unsupported devices.
// To enable, CrostiniUsbSupport must also be enabled.
const base::Feature kCrostiniUsbAllowUnsupported{
    "CrostiniUsbAllowUnsupported", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 Enrollment flow.
const base::Feature kCryptAuthV2Enrollment{"CryptAuthV2Enrollment",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Discover Application on Chrome OS.
// If enabled, Discover App will be shown in launcher.
const base::Feature kDiscoverApp{"DiscoverApp",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, DriveFS will be used for Drive sync.
const base::Feature kDriveFs{"DriveFS", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled shows the visual signals feedback panel.
const base::Feature kEnableFileManagerFeedbackPanel{
    "EnableFeedbackPanel", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the enhanced external media formatting dialog in the file manager,
// with support for labelling and also NTFS/exFAT filesystems.
const base::Feature kEnableFileManagerFormatDialog{
    "EnableFileManagerFormatDialog", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the piex-wasm module for raw image preview image extraction.
const base::Feature kEnableFileManagerPiexWasm{
    "PiexWasm", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables web push for background notifications in
// Android Messages Integration on Chrome OS.
const base::Feature kEnableMessagesWebPush{"EnableMessagesWebPush",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the use of Mojo by Chrome-process code to communicate with Power
// Manager. In order to use mojo, this feature must be turned on and a callsite
// must use PowerManagerMojoClient::Get().
const base::Feature kMojoDBusRelay{"MojoDBusRelay",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, MyFiles will be a root/volume and user can create other
// sub-folders and files in addition to the Downloads folder inside MyFiles.
const base::Feature kMyFilesVolume{"MyFilesVolume",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

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

// Enable or disable Unified Input Logic for FST decocder in the IME extension
// on Chrome OS.
const base::Feature kImeInputLogicFst{"ImeInputLogicFst",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Instant Tethering on Chrome OS.
const base::Feature kInstantTethering{"InstantTethering",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables showing the battery level in the System Tray and Settings
// UI for supported Bluetooth Devices.
const base::Feature kShowBluetoothDeviceBattery{
    "ShowBluetoothDeviceBattery", base::FEATURE_DISABLED_BY_DEFAULT};

// Uses the V3 (~2019-05 era) Smart Dim model instead of the default V2
// (~2018-11) model.
const base::Feature kSmartDimModelV3{"SmartDimModelV3",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Splits OS settings (display, mouse, keyboard, etc.) out from browser settings
// into a separate window.
const base::Feature kSplitSettings{"SplitSettings",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, a new screen will be shown at the end of the OOBE/Login for
// supervised users. It will display a loading page while we fetch
// eligibility data.
const base::Feature kSupervisionOnboardingEligibility{
    "SupervisionOnboardingEligibility", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, we will display the full onboarding flow for eligible supervised
// users.
const base::Feature kSupervisionOnboardingScreens{
    "SupervisionOnboardingScreens", base::FEATURE_DISABLED_BY_DEFAULT};

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

bool IsSplitSettingsEnabled() {
  return base::FeatureList::IsEnabled(kSplitSettings);
}

}  // namespace features

}  // namespace chromeos
