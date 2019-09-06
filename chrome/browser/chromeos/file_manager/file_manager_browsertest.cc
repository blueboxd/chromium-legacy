// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_base.h"

namespace file_manager {

// TestCase: FilesAppBrowserTest parameters.
struct TestCase {
  explicit TestCase(const char* name) : test_case_name(name) {
    CHECK(name) << "FATAL: no test case name";
  }

  TestCase& InGuestMode() {
    guest_mode = IN_GUEST_MODE;
    return *this;
  }

  TestCase& InIncognito() {
    guest_mode = IN_INCOGNITO;
    return *this;
  }

  TestCase& TabletMode() {
    tablet_mode = true;
    return *this;
  }

  TestCase& EnableDriveFs() {
    enable_drivefs.emplace(true);
    return *this;
  }

  TestCase& DisableDriveFs() {
    enable_drivefs.emplace(false);
    return *this;
  }

  TestCase& EnableDocumentsProvider() {
    enable_arc = true;
    enable_documents_provider.emplace(true);
    return *this;
  }

  TestCase& DisableDocumentsProvider() {
    enable_documents_provider.emplace(false);
    return *this;
  }

  TestCase& EnableFormatDialog() {
    enable_format_dialog.emplace(true);
    return *this;
  }

  TestCase& EnableArc() {
    enable_arc = true;
    return *this;
  }

  TestCase& Offline() {
    offline = true;
    return *this;
  }

  TestCase& FilesNg() {
    files_ng = true;
    return *this;
  }

  TestCase& DisableNativeSmb() {
    enable_native_smb = false;
    return *this;
  }

  TestCase& DontMountVolumes() {
    mount_no_volumes = true;
    return *this;
  }

  // Show the startup browser. Some tests invoke the file picker dialog during
  // the test. Requesting a file picker from a background page is forbidden by
  // the apps platform, and it's a bug that these tests do so.
  // FindRuntimeContext() in select_file_dialog_extension.cc will use the last
  // active browser in this case, which requires a Browser to be present. See
  // https://crbug.com/736930.
  TestCase& WithBrowser() {
    with_browser = true;
    return *this;
  }

  static std::string GetFullTestCaseName(const TestCase& test) {
    std::string name(test.test_case_name);

    CHECK(!name.empty()) << "FATAL: no test case name.";

    if (test.guest_mode == IN_GUEST_MODE)
      name.append("_GuestMode");
    else if (test.guest_mode == IN_INCOGNITO)
      name.append("_Incognito");

    if (test.tablet_mode)
      name.append("_TabletMode");

    if (test.enable_drivefs.value_or(false))
      name.append("_DriveFs");

    if (test.files_ng)
      name.append("_FilesNg");

    if (!test.enable_native_smb)
      name.append("_DisableNativeSmb");

    if (test.enable_documents_provider.value_or(false))
      name.append("_DocumentsProvider");

    return name;
  }

  const char* test_case_name = nullptr;
  GuestMode guest_mode = NOT_IN_GUEST_MODE;
  bool tablet_mode = false;
  base::Optional<bool> enable_drivefs;
  base::Optional<bool> enable_myfiles_volume;
  base::Optional<bool> enable_documents_provider;
  base::Optional<bool> enable_format_dialog;
  bool enable_arc = false;
  bool with_browser = false;
  bool needs_zip = false;
  bool offline = false;
  bool files_ng = false;
  bool enable_native_smb = true;
  bool mount_no_volumes = false;
};

// ZipCase: FilesAppBrowserTest with zip/unzip support.
struct ZipCase : public TestCase {
  explicit ZipCase(const char* name) : TestCase(name) { needs_zip = true; }
};

// FilesApp browser test.
class FilesAppBrowserTest : public FileManagerBrowserTestBase,
                            public ::testing::WithParamInterface<TestCase> {
 public:
  FilesAppBrowserTest() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileManagerBrowserTestBase::SetUpCommandLine(command_line);
    // Default mode is clamshell: force Ash into tablet mode if requested,
    // and enable the Ash virtual keyboard sub-system therein.
    if (GetParam().tablet_mode) {
      command_line->AppendSwitchASCII("force-tablet-mode", "touch_view");
      command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
    }
  }

  GuestMode GetGuestMode() const override { return GetParam().guest_mode; }

  const char* GetTestCaseName() const override {
    return GetParam().test_case_name;
  }

  std::string GetFullTestCaseName() const override {
    return TestCase::GetFullTestCaseName(GetParam());
  }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  bool GetTabletMode() const override { return GetParam().tablet_mode; }

  bool GetEnableDriveFs() const override {
    return GetParam().enable_drivefs.value_or(
        FileManagerBrowserTestBase::GetEnableDriveFs());
  }

  bool GetEnableDocumentsProvider() const override {
    return GetParam().enable_documents_provider.value_or(
        FileManagerBrowserTestBase::GetEnableDocumentsProvider());
  }

  bool GetEnableFormatDialog() const override {
    return GetParam().enable_format_dialog.value_or(
        FileManagerBrowserTestBase::GetEnableFormatDialog());
  }

  bool GetEnableArc() const override { return GetParam().enable_arc; }

  bool GetRequiresStartupBrowser() const override {
    return GetParam().with_browser;
  }

  bool GetNeedsZipSupport() const override { return GetParam().needs_zip; }

  bool GetIsOffline() const override { return GetParam().offline; }

  bool GetEnableFilesNg() const override { return GetParam().files_ng; }

  bool GetEnableNativeSmb() const override {
    return GetParam().enable_native_smb;
  }

  bool GetStartWithNoVolumesMounted() const override {
    return GetParam().mount_no_volumes;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FilesAppBrowserTest);
};

IN_PROC_BROWSER_TEST_P(FilesAppBrowserTest, Test) {
  StartTest();
}

// INSTANTIATE_TEST_SUITE_P expands to code that stringizes the arguments. Thus
// macro parameters such as |prefix| and |test_class| won't be expanded by the
// macro pre-processor. To work around this, indirect INSTANTIATE_TEST_SUITE_P,
// as WRAPPED_INSTANTIATE_TEST_SUITE_P here, so the pre-processor expands macro
// defines used to disable tests, MAYBE_prefix for example.
#define WRAPPED_INSTANTIATE_TEST_SUITE_P(prefix, test_class, generator) \
  INSTANTIATE_TEST_SUITE_P(prefix, test_class, generator, &PostTestCaseName)

std::string PostTestCaseName(const ::testing::TestParamInfo<TestCase>& test) {
  return TestCase::GetFullTestCaseName(test.param);
}

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileDisplay, /* file_display.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fileDisplayDownloads"),
        TestCase("fileDisplayDownloads").InGuestMode(),
        TestCase("fileDisplayDownloads").TabletMode(),
        TestCase("fileDisplayDrive").DisableDriveFs(),
        TestCase("fileDisplayDrive").TabletMode().DisableDriveFs(),
        TestCase("fileDisplayDrive").TabletMode().EnableDriveFs(),
        TestCase("fileDisplayDrive").EnableDriveFs(),
        TestCase("fileDisplayDriveOffline").Offline().EnableDriveFs(),
        TestCase("fileDisplayDriveOnline").EnableDriveFs(),
        TestCase("fileDisplayDriveOnline").DisableDriveFs(),
        TestCase("fileDisplayComputers").EnableDriveFs(),
        TestCase("fileDisplayMtp"),
        TestCase("fileDisplayUsb"),
        TestCase("fileDisplayUsbPartition"),
        TestCase("fileDisplayPartitionFileTable"),
        TestCase("fileSearch"),
        TestCase("fileDisplayWithoutDownloadsVolume").DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumes").DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumesThenMountDownloads")

            .DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumesThenMountDrive")
            .DontMountVolumes()
            .EnableDriveFs(),
        TestCase("fileDisplayWithoutDrive").DontMountVolumes(),
        TestCase("fileDisplayWithoutDriveThenDisable").DontMountVolumes(),
        TestCase("fileDisplayMountWithFakeItemSelected"),
        TestCase("fileDisplayUnmountDriveWithSharedWithMeSelected"),
        TestCase("fileDisplayUnmountRemovableRoot"),
        TestCase("fileDisplayUnmountFirstPartition"),
        TestCase("fileDisplayUnmountLastPartition"),
        TestCase("fileSearchCaseInsensitive"),
        TestCase("fileSearchNotFound"),
        TestCase("fileDisplayDownloadsWithBlockedFileTaskRunner"),
        TestCase("fileDisplayCheckSelectWithFakeItemSelected"),
        TestCase("fileDisplayCheckReadOnlyIconOnFakeDirectory"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnDownloads")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenVideoFiles, /* open_video_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("videoOpenDownloads").InGuestMode(),
                      TestCase("videoOpenDownloads"),
                      TestCase("videoOpenDrive").DisableDriveFs(),
                      TestCase("videoOpenDrive").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenAudioFiles, /* open_audio_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("audioOpenCloseDownloads"),
                      TestCase("audioOpenCloseDownloads").InGuestMode(),
                      TestCase("audioOpenCloseDrive"),
                      TestCase("audioOpenDownloads").InGuestMode(),
                      TestCase("audioOpenDownloads"),
                      TestCase("audioOpenDrive").DisableDriveFs(),
                      TestCase("audioOpenDrive").EnableDriveFs(),
                      TestCase("audioAutoAdvanceDrive"),
                      TestCase("audioRepeatAllModeSingleFileDrive"),
                      TestCase("audioNoRepeatModeSingleFileDrive"),
                      TestCase("audioRepeatOneModeSingleFileDrive"),
                      TestCase("audioRepeatAllModeMultipleFileDrive"),
                      TestCase("audioNoRepeatModeMultipleFileDrive"),
                      TestCase("audioRepeatOneModeMultipleFileDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenImageFiles, /* open_image_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("imageOpenDownloads").InGuestMode(),
                      TestCase("imageOpenDownloads"),
                      TestCase("imageOpenDrive").DisableDriveFs(),
                      TestCase("imageOpenDrive").EnableDriveFs(),
                      TestCase("imageOpenGalleryOpenDownloads"),
                      TestCase("imageOpenGalleryOpenDrive").DisableDriveFs(),
                      TestCase("imageOpenGalleryOpenDrive").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenSniffedFiles, /* open_sniffed_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("pdfOpenDownloads"),
                      TestCase("pdfOpenDrive").EnableDriveFs(),
                      TestCase("textOpenDownloads"),
                      TestCase("textOpenDrive").EnableDriveFs()));

// NaCl fails to compile zip plugin.pexe too often on ASAN, crbug.com/867738
// The tests are flaky on the debug bot and always time out first and then pass
// on retry. Disabled for debug as per crbug.com/936429.
#if defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
#define MAYBE_ZipFiles DISABLED_ZipFiles
#else
#define MAYBE_ZipFiles ZipFiles
#endif
WRAPPED_INSTANTIATE_TEST_SUITE_P(
    MAYBE_ZipFiles, /* zip_files.js */
    FilesAppBrowserTest,
    ::testing::Values(ZipCase("zipFileOpenDownloads").InGuestMode(),
                      ZipCase("zipFileOpenDownloads"),
                      ZipCase("zipFileOpenDownloadsShiftJIS"),
                      ZipCase("zipFileOpenDownloadsMacOs"),
                      ZipCase("zipFileOpenDownloadsWithAbsolutePaths"),
                      ZipCase("zipFileOpenDownloadsEncryptedCancelPassphrase"),
                      ZipCase("zipFileOpenDrive").DisableDriveFs(),
                      ZipCase("zipFileOpenDrive").EnableDriveFs(),
                      ZipCase("zipFileOpenUsb"),
                      ZipCase("zipCreateFileDownloads").InGuestMode(),
                      ZipCase("zipCreateFileDownloads"),
                      ZipCase("zipCreateFileDrive").DisableDriveFs(),
                      ZipCase("zipCreateFileDrive").EnableDriveFs(),
                      ZipCase("zipCreateFileUsb")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CreateNewFolder, /* create_new_folder.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("selectCreateFolderDownloads").InGuestMode(),
                      TestCase("selectCreateFolderDownloads"),
                      TestCase("createFolderDownloads").InGuestMode(),
                      TestCase("createFolderDownloads"),
                      TestCase("createFolderNestedDownloads"),
                      TestCase("createFolderDrive").DisableDriveFs(),
                      TestCase("createFolderDrive").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    KeyboardOperations, /* keyboard_operations.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("keyboardDeleteDownloads").InGuestMode(),
                      TestCase("keyboardDeleteDownloads"),
                      TestCase("keyboardDeleteDrive").DisableDriveFs(),
                      TestCase("keyboardDeleteDrive").EnableDriveFs(),
                      TestCase("keyboardDeleteFolderDownloads").InGuestMode(),
                      TestCase("keyboardDeleteFolderDownloads"),
                      TestCase("keyboardDeleteFolderDrive").DisableDriveFs(),
                      TestCase("keyboardDeleteFolderDrive").EnableDriveFs(),
                      TestCase("keyboardCopyDownloads").InGuestMode(),
                      TestCase("keyboardCopyDownloads"),
                      TestCase("keyboardCopyDrive").DisableDriveFs(),
                      TestCase("keyboardCopyDrive").EnableDriveFs(),
                      TestCase("keyboardSelectDriveDirectoryTree"),
                      TestCase("keyboardDisableCopyWhenDialogDisplayed"),
                      TestCase("keyboardOpenNewWindow"),
                      TestCase("keyboardOpenNewWindow").InGuestMode(),
                      TestCase("renameFileDownloads").InGuestMode(),
                      TestCase("renameFileDownloads"),
                      TestCase("renameFileDrive").DisableDriveFs(),
                      TestCase("renameFileDrive").EnableDriveFs(),
                      TestCase("renameNewFolderDownloads").InGuestMode(),
                      TestCase("renameNewFolderDownloads"),
                      TestCase("renameNewFolderDrive").DisableDriveFs(),
                      TestCase("renameNewFolderDrive").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ContextMenu, /* context_menu.js for file list */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("checkDeleteEnabledForReadWriteFile").DisableDriveFs(),
        TestCase("checkDeleteDisabledForReadOnlyDocument").DisableDriveFs(),
        TestCase("checkDeleteDisabledForReadOnlyFile").DisableDriveFs(),
        TestCase("checkDeleteDisabledForReadOnlyFolder").DisableDriveFs(),
        TestCase("checkRenameEnabledForReadWriteFile").DisableDriveFs(),
        TestCase("checkRenameDisabledForReadOnlyDocument").DisableDriveFs(),
        TestCase("checkRenameDisabledForReadOnlyFile").DisableDriveFs(),
        TestCase("checkRenameDisabledForReadOnlyFolder").DisableDriveFs(),
        TestCase("checkShareEnabledForReadWriteFile").DisableDriveFs(),
        TestCase("checkShareEnabledForReadOnlyDocument").DisableDriveFs(),
        TestCase("checkShareDisabledForStrictReadOnlyDocument")
            .DisableDriveFs(),
        TestCase("checkShareEnabledForReadOnlyFile").DisableDriveFs(),
        TestCase("checkShareEnabledForReadOnlyFolder").DisableDriveFs(),
        TestCase("checkCopyEnabledForReadWriteFile").DisableDriveFs(),
        TestCase("checkCopyEnabledForReadOnlyDocument").DisableDriveFs(),
        TestCase("checkCopyDisabledForStrictReadOnlyDocument").DisableDriveFs(),
        TestCase("checkCopyEnabledForReadOnlyFile").DisableDriveFs(),
        TestCase("checkCopyEnabledForReadOnlyFolder").DisableDriveFs(),
        TestCase("checkCutEnabledForReadWriteFile").DisableDriveFs(),
        TestCase("checkCutDisabledForReadOnlyDocument").DisableDriveFs(),
        TestCase("checkCutDisabledForReadOnlyFile").DisableDriveFs(),
        TestCase("checkCutDisabledForReadOnlyFolder").DisableDriveFs(),
        TestCase("checkPasteIntoFolderEnabledForReadWriteFolder")
            .DisableDriveFs(),
        TestCase("checkPasteIntoFolderDisabledForReadOnlyFolder")
            .DisableDriveFs(),
        TestCase("checkContextMenusForInputElements"),
        TestCase("checkNewFolderEnabledInsideReadWriteFolder").DisableDriveFs(),
        TestCase("checkNewFolderDisabledInsideReadOnlyFolder").DisableDriveFs(),
        TestCase("checkPasteEnabledInsideReadWriteFolder").DisableDriveFs(),
        TestCase("checkPasteDisabledInsideReadOnlyFolder").DisableDriveFs()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ContextMenu2, /* context_menu.js for file list */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("checkDeleteEnabledForReadWriteFile").EnableDriveFs(),
        TestCase("checkDeleteDisabledForReadOnlyDocument").EnableDriveFs(),
        TestCase("checkDeleteDisabledForReadOnlyFile").EnableDriveFs(),
        TestCase("checkDeleteDisabledForReadOnlyFolder").EnableDriveFs(),
        TestCase("checkRenameEnabledForReadWriteFile").EnableDriveFs(),
        TestCase("checkRenameDisabledForReadOnlyDocument").EnableDriveFs(),
        TestCase("checkRenameDisabledForReadOnlyFile").EnableDriveFs(),
        TestCase("checkRenameDisabledForReadOnlyFolder").EnableDriveFs(),
        TestCase("checkShareEnabledForReadWriteFile").EnableDriveFs(),
        TestCase("checkShareEnabledForReadOnlyDocument").EnableDriveFs(),
        TestCase("checkShareDisabledForStrictReadOnlyDocument").EnableDriveFs(),
        TestCase("checkShareEnabledForReadOnlyFile").EnableDriveFs(),
        TestCase("checkShareEnabledForReadOnlyFolder").EnableDriveFs(),
        TestCase("checkCopyEnabledForReadWriteFile").EnableDriveFs(),
        TestCase("checkCopyEnabledForReadOnlyDocument").EnableDriveFs(),
        TestCase("checkCopyDisabledForStrictReadOnlyDocument").EnableDriveFs(),
        TestCase("checkCopyEnabledForReadOnlyFile").EnableDriveFs(),
        TestCase("checkCopyEnabledForReadOnlyFolder").EnableDriveFs(),
        TestCase("checkCutEnabledForReadWriteFile").EnableDriveFs(),
        TestCase("checkCutDisabledForReadOnlyDocument").EnableDriveFs(),
        TestCase("checkCutDisabledForReadOnlyFile").EnableDriveFs(),
        TestCase("checkCutDisabledForReadOnlyFolder").EnableDriveFs(),
        TestCase("checkPasteIntoFolderEnabledForReadWriteFolder")
            .EnableDriveFs(),
        TestCase("checkPasteIntoFolderDisabledForReadOnlyFolder")
            .EnableDriveFs(),
        TestCase("checkInstallWithLinuxDisabledForDebianFile").EnableDriveFs(),
        TestCase("checkInstallWithLinuxEnabledForDebianFile").EnableDriveFs(),
        TestCase("checkNewFolderEnabledInsideReadWriteFolder").EnableDriveFs(),
        TestCase("checkNewFolderDisabledInsideReadOnlyFolder").EnableDriveFs(),
        TestCase("checkPasteEnabledInsideReadWriteFolder").EnableDriveFs(),
        TestCase("checkPasteDisabledInsideReadOnlyFolder").EnableDriveFs(),
        TestCase("checkDownloadsContextMenu"),
        TestCase("checkPlayFilesContextMenu"),
        TestCase("checkLinuxFilesContextMenu"),
        TestCase("checkDeleteDisabledInDocProvider").EnableDocumentsProvider(),
        TestCase("checkDeleteEnabledInDocProvider").EnableDocumentsProvider(),
        TestCase("checkRenameDisabledInDocProvider").EnableDocumentsProvider(),
        TestCase("checkRenameEnabledInDocProvider").EnableDocumentsProvider(),
        TestCase("checkContextMenuFocus")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Toolbar, /* toolbar.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("toolbarDeleteWithMenuItemNoEntrySelected"),
                      TestCase("toolbarDeleteEntry").InGuestMode(),
                      TestCase("toolbarDeleteEntry"),
                      TestCase("toolbarRefreshButtonWithSelection").EnableArc(),
                      TestCase("toolbarRefreshButtonHiddenInRecents")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    QuickView, /* quick_view.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openQuickView"),
        TestCase("openQuickViewAndEscape"),
        TestCase("openQuickView").InGuestMode(),
        TestCase("openQuickView").TabletMode(),
        TestCase("openQuickViewAudio"),
        TestCase("openQuickViewImage"),
        TestCase("openQuickViewImageExif"),
        TestCase("openQuickViewImageRaw"),
        TestCase("openQuickViewVideo"),
// QuickView PDF test fails on MSAN, crbug.com/768070
#if !defined(MEMORY_SANITIZER)
        TestCase("openQuickViewPdf"),
#endif
        TestCase("openQuickViewKeyboardUpDownChangesView"),
        TestCase("openQuickViewKeyboardLeftRightChangesView"),
        TestCase("openQuickViewSniffedText"),
        TestCase("openQuickViewScrollText"),
        TestCase("openQuickViewScrollHtml"),
        TestCase("openQuickViewBackgroundColorText"),
        TestCase("openQuickViewBackgroundColorHtml"),
        TestCase("openQuickViewDrive").DisableDriveFs(),
        TestCase("openQuickViewDrive").EnableDriveFs(),
        TestCase("openQuickViewAndroid"),
        TestCase("openQuickViewDocumentsProvider").EnableDocumentsProvider(),
        TestCase("openQuickViewCrostini"),
        TestCase("openQuickViewUsb"),
        TestCase("openQuickViewRemovablePartitions"),
        TestCase("openQuickViewMtp"),
        TestCase("pressEnterOnInfoBoxToOpenClose"),
        TestCase("closeQuickView"),
        TestCase("cantOpenQuickViewWithMultipleFiles"),
        TestCase("openQuickViewFromDirectoryTree")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTree, /* directory_tree.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("directoryTreeHorizontalScroll"),
                      TestCase("directoryTreeVerticalScroll")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTreeContextMenu, /* directory_tree_context_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("dirCopyWithContextMenu").InGuestMode(),
        TestCase("dirCopyWithContextMenu"),
        TestCase("dirCopyWithKeyboard").InGuestMode(),
        TestCase("dirCopyWithKeyboard"),
        TestCase("dirCopyWithoutChangingCurrent"),
        TestCase("dirCutWithContextMenu").InGuestMode(),
        TestCase("dirCutWithContextMenu"),
        TestCase("dirCutWithKeyboard").InGuestMode(),
        TestCase("dirCutWithKeyboard"),
        TestCase("dirPasteWithContextMenu").InGuestMode(),
        TestCase("dirPasteWithContextMenu"),
        TestCase("dirPasteWithoutChangingCurrent"),
        TestCase("dirRenameWithContextMenu").InGuestMode(),
        TestCase("dirRenameWithContextMenu"),
        TestCase("dirRenameUpdateChildrenBreadcrumbs"),
        TestCase("dirRenameWithKeyboard").InGuestMode(),
        TestCase("dirRenameWithKeyboard"),
        TestCase("dirRenameWithoutChangingCurrent"),
        TestCase("dirRenameToEmptyString").InGuestMode(),
        TestCase("dirRenameToEmptyString"),
        TestCase("dirRenameToExisting").InGuestMode(),
        TestCase("dirRenameToExisting").EnableDriveFs(),
        TestCase("dirRenameRemovableWithKeyboard"),
        TestCase("dirRenameRemovableWithKeyboard").InGuestMode(),
        TestCase("dirRenameRemovableWithKeyboard").EnableDriveFs(),
        TestCase("dirRenameRemovableWithContentMenu"),
        TestCase("dirRenameRemovableWithContentMenu").InGuestMode(),
        TestCase("dirRenameRemovableWithContentMenu").EnableDriveFs(),
        TestCase("dirCreateWithContextMenu"),
        TestCase("dirCreateWithKeyboard"),
        TestCase("dirCreateWithoutChangingCurrent"),
#if !(defined(ADDRESS_SANITIZER) || !defined(NDEBUG))
        // Zip tests times out too often on ASAN and DEBUG. crbug.com/936429
        // and crbug.com/944697
        ZipCase("dirContextMenuZip"),
        ZipCase("dirEjectContextMenuZip"),
#endif
        TestCase("dirContextMenuRecent"),
        TestCase("dirContextMenuMyFiles"),
        TestCase("dirContextMenuMyFilesWithPaste"),
        TestCase("dirContextMenuCrostini"),
        TestCase("dirContextMenuPlayFiles"),
        TestCase("dirContextMenuUsbs"),
        TestCase("dirContextMenuFsp"),
        TestCase("dirContextMenuDocumentsProvider").EnableDocumentsProvider(),
        TestCase("dirContextMenuUsbDcim"),
        TestCase("dirContextMenuMtp"),
        TestCase("dirContextMenuMediaView").EnableArc(),
        TestCase("dirContextMenuMyDrive"),
        TestCase("dirContextMenuSharedDrive"),
        TestCase("dirContextMenuSharedWithMe"),
        TestCase("dirContextMenuOffline"),
        TestCase("dirContextMenuComputers"),
        TestCase("dirContextMenuShortcut"),
        TestCase("dirContextMenuFocus")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DriveSpecific, /* drive_specific.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("driveOpenSidebarOffline").DisableDriveFs(),
        TestCase("driveOpenSidebarOffline").EnableDriveFs(),
        TestCase("driveOpenSidebarSharedWithMe").DisableDriveFs(),
        TestCase("driveOpenSidebarSharedWithMe").EnableDriveFs(),
        TestCase("driveAutoCompleteQuery").DisableDriveFs(),
        TestCase("driveAutoCompleteQuery").EnableDriveFs(),
        TestCase("drivePinFileMobileNetwork").DisableDriveFs(),
        TestCase("drivePinFileMobileNetwork").EnableDriveFs(),
        TestCase("driveClickFirstSearchResult").DisableDriveFs(),
        TestCase("driveClickFirstSearchResult").EnableDriveFs(),
        TestCase("drivePressEnterToSearch").DisableDriveFs(),
        TestCase("drivePressEnterToSearch").EnableDriveFs(),
        TestCase("drivePressClearSearch").EnableDriveFs(),
        TestCase("drivePressCtrlAFromSearch").DisableDriveFs(),
        TestCase("drivePressCtrlAFromSearch").EnableDriveFs(),
        TestCase("driveBackupPhotos").DisableDriveFs(),
        TestCase("driveBackupPhotos").EnableDriveFs(),
        TestCase("driveAvailableOfflineGearMenu").DisableDriveFs(),
        TestCase("driveAvailableOfflineGearMenu").EnableDriveFs(),
        TestCase("driveAvailableOfflineDirectoryGearMenu"),
        TestCase("driveLinkToDirectory").EnableDriveFs(),
        TestCase("driveLinkOpenFileThroughLinkedDirectory").EnableDriveFs(),
        TestCase("driveLinkOpenFileThroughTransitiveLink").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Transfer, /* transfer.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("transferFromDriveToDownloads").DisableDriveFs(),
        TestCase("transferFromDriveToDownloads").EnableDriveFs(),
        TestCase("transferFromDownloadsToMyFiles"),
        TestCase("transferFromDownloadsToMyFilesMove"),
        TestCase("transferFromDownloadsToDrive").DisableDriveFs(),
        TestCase("transferFromDownloadsToDrive").EnableDriveFs(),
        TestCase("transferFromSharedToDownloads").DisableDriveFs(),
        TestCase("transferFromSharedToDownloads").EnableDriveFs(),
        TestCase("transferFromSharedToDrive").DisableDriveFs(),
        TestCase("transferFromSharedToDrive").EnableDriveFs(),
        TestCase("transferFromOfflineToDownloads").DisableDriveFs(),
        TestCase("transferFromOfflineToDownloads").EnableDriveFs(),
        TestCase("transferFromOfflineToDrive").DisableDriveFs(),
        TestCase("transferFromOfflineToDrive").EnableDriveFs(),
        TestCase("transferFromTeamDriveToDrive").DisableDriveFs(),
        TestCase("transferFromTeamDriveToDrive").EnableDriveFs(),
        TestCase("transferFromDriveToTeamDrive").DisableDriveFs(),
        TestCase("transferFromDriveToTeamDrive").EnableDriveFs(),
        TestCase("transferFromTeamDriveToDownloads").DisableDriveFs(),
        TestCase("transferFromTeamDriveToDownloads").EnableDriveFs(),
        TestCase("transferHostedFileFromTeamDriveToDownloads").DisableDriveFs(),
        TestCase("transferHostedFileFromTeamDriveToDownloads").EnableDriveFs(),
        TestCase("transferFromDownloadsToTeamDrive").DisableDriveFs(),
        TestCase("transferFromDownloadsToTeamDrive").EnableDriveFs(),
        TestCase("transferBetweenTeamDrives").DisableDriveFs(),
        TestCase("transferBetweenTeamDrives").EnableDriveFs(),
        TestCase("transferDragAndDrop"),
        TestCase("transferDragAndHover"),
        TestCase("transferFromDownloadsToDownloads"),
        TestCase("transferDeletedFile")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    RestorePrefs, /* restore_prefs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("restoreSortColumn").InGuestMode(),
                      TestCase("restoreSortColumn"),
                      TestCase("restoreCurrentView").InGuestMode(),
                      TestCase("restoreCurrentView")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    RestoreGeometry, /* restore_geometry.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("restoreGeometry"),
                      TestCase("restoreGeometry").InGuestMode(),
                      TestCase("restoreGeometryMaximized")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ShareAndManageDialog, /* share_and_manage_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("shareFileDrive").DisableDriveFs(),
                      TestCase("shareFileDrive").EnableDriveFs(),
                      TestCase("shareDirectoryDrive").DisableDriveFs(),
                      TestCase("shareDirectoryDrive").EnableDriveFs(),
                      TestCase("shareHostedFileDrive"),
                      TestCase("manageHostedFileDrive").DisableDriveFs(),
                      TestCase("manageHostedFileDrive").EnableDriveFs(),
                      TestCase("manageFileDrive").DisableDriveFs(),
                      TestCase("manageFileDrive").EnableDriveFs(),
                      TestCase("manageDirectoryDrive").DisableDriveFs(),
                      TestCase("manageDirectoryDrive").EnableDriveFs(),
                      TestCase("shareFileTeamDrive"),
                      TestCase("shareDirectoryTeamDrive"),
                      TestCase("shareHostedFileTeamDrive"),
                      TestCase("shareTeamDrive"),
                      TestCase("manageHostedFileTeamDrive"),
                      TestCase("manageFileTeamDrive"),
                      TestCase("manageDirectoryTeamDrive"),
                      TestCase("manageTeamDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    SuggestAppDialog, /* suggest_app_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("suggestAppDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Traverse, /* traverse.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseDownloads").InGuestMode(),
                      TestCase("traverseDownloads"),
                      TestCase("traverseDrive").DisableDriveFs(),
                      TestCase("traverseDrive").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Tasks, /* tasks.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("executeDefaultTaskDownloads"),
                      TestCase("executeDefaultTaskDownloads").InGuestMode(),
                      TestCase("executeDefaultTaskDrive").DisableDriveFs(),
                      TestCase("executeDefaultTaskDrive").EnableDriveFs(),
                      TestCase("defaultTaskForPdf"),
                      TestCase("defaultTaskForTextPlain"),
                      TestCase("defaultTaskDialogDownloads"),
                      TestCase("defaultTaskDialogDownloads").InGuestMode(),
                      TestCase("defaultTaskDialogDrive").DisableDriveFs(),
                      TestCase("defaultTaskDialogDrive").EnableDriveFs(),
                      TestCase("genericTaskIsNotExecuted"),
                      TestCase("genericTaskAndNonGenericTask")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FolderShortcuts, /* folder_shortcuts.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseFolderShortcuts").DisableDriveFs(),
                      TestCase("traverseFolderShortcuts").EnableDriveFs(),
                      TestCase("addRemoveFolderShortcuts").DisableDriveFs(),
                      TestCase("addRemoveFolderShortcuts").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    SortColumns, /* sort_columns.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("sortColumns"),
                      TestCase("sortColumns").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    TabIndex, /* tab_index.js: */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("tabindexSearchBoxFocus"),
        TestCase("tabindexFocus"),
        TestCase("tabindexFocusDownloads"),
        TestCase("tabindexFocusDownloads").InGuestMode(),
        TestCase("tabindexFocusBreadcrumbBackground"),
        TestCase("tabindexFocusDirectorySelected"),
        TestCase("tabindexOpenDialogDrive").WithBrowser().DisableDriveFs(),
        TestCase("tabindexOpenDialogDownloads").WithBrowser(),
        TestCase("tabindexOpenDialogDownloads").WithBrowser().InGuestMode(),
        TestCase("tabindexSaveFileDialogDrive").WithBrowser().DisableDriveFs(),
        TestCase("tabindexSaveFileDialogDrive").WithBrowser().EnableDriveFs(),
        TestCase("tabindexSaveFileDialogDownloads").WithBrowser(),
        TestCase("tabindexSaveFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileDialog, /* file_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openFileDialogUnload").WithBrowser(),
        TestCase("openFileDialogDownloads").WithBrowser(),
        TestCase("openFileDialogDownloads").WithBrowser().InGuestMode(),
        TestCase("openFileDialogDownloads").WithBrowser().InIncognito(),
        TestCase("openFileDialogPanelsDisabled").WithBrowser(),
        TestCase("saveFileDialogDownloads").WithBrowser(),
        TestCase("saveFileDialogDownloads").WithBrowser().InGuestMode(),
        TestCase("saveFileDialogDownloads").WithBrowser().InIncognito(),
        TestCase("saveFileDialogDownloadsNewFolderButton").WithBrowser(),
        TestCase("saveFileDialogPanelsDisabled").WithBrowser(),
        TestCase("openFileDialogCancelDownloads").WithBrowser(),
        TestCase("openFileDialogEscapeDownloads").WithBrowser(),
        TestCase("openFileDialogDrive").WithBrowser().DisableDriveFs(),
        TestCase("openFileDialogDrive")
            .WithBrowser()
            .InIncognito()
            .DisableDriveFs(),
        TestCase("openFileDialogDrive").WithBrowser().EnableDriveFs(),
        TestCase("openFileDialogDrive")
            .WithBrowser()
            .InIncognito()
            .EnableDriveFs(),
        TestCase("saveFileDialogDrive").WithBrowser(),
        TestCase("saveFileDialogDrive").WithBrowser().InIncognito(),
        TestCase("openFileDialogDriveFromBrowser")
            .WithBrowser()
            .EnableDriveFs(),
        TestCase("openFileDialogDriveFromBrowser")
            .WithBrowser()
            .DisableDriveFs(),
        TestCase("openFileDialogDriveHostedDoc").WithBrowser().EnableDriveFs(),
        TestCase("openFileDialogDriveHostedDoc").WithBrowser().DisableDriveFs(),
        TestCase("openFileDialogDriveHostedNeedsFile")
            .WithBrowser()
            .EnableDriveFs(),
        TestCase("saveFileDialogDriveHostedNeedsFile")
            .WithBrowser()
            .EnableDriveFs(),
        TestCase("openFileDialogDriveHostedNeedsFile")
            .WithBrowser()
            .DisableDriveFs(),
        TestCase("openFileDialogCancelDrive").WithBrowser().DisableDriveFs(),
        TestCase("openFileDialogCancelDrive").WithBrowser().EnableDriveFs(),
        TestCase("openFileDialogEscapeDrive").WithBrowser().DisableDriveFs(),
        TestCase("openFileDialogEscapeDrive").WithBrowser().EnableDriveFs(),
        TestCase("openFileDialogDriveOffline")
            .WithBrowser()
            .Offline()
            .EnableDriveFs(),
        TestCase("saveFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("openFileDialogDriveOfflinePinned")
            .WithBrowser()
            .Offline()
            .EnableDriveFs(),
        TestCase("saveFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("openFileDialogDefaultFilter").WithBrowser(),
        TestCase("saveFileDialogDefaultFilter").WithBrowser(),
        TestCase("openFileDialogFileListShowContextMenu").WithBrowser()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CopyBetweenWindows, /* copy_between_windows.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("copyBetweenWindowsLocalToDrive").DisableDriveFs(),
        TestCase("copyBetweenWindowsLocalToDrive").EnableDriveFs(),
        TestCase("copyBetweenWindowsLocalToUsb"),
        TestCase("copyBetweenWindowsUsbToDrive").DisableDriveFs(),
        TestCase("copyBetweenWindowsUsbToDrive").EnableDriveFs(),
        TestCase("copyBetweenWindowsDriveToLocal").DisableDriveFs(),
        TestCase("copyBetweenWindowsDriveToLocal").EnableDriveFs(),
        TestCase("copyBetweenWindowsDriveToUsb").DisableDriveFs(),
        TestCase("copyBetweenWindowsDriveToUsb").EnableDriveFs(),
        TestCase("copyBetweenWindowsUsbToLocal")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GridView, /* grid_view.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("showGridViewDownloads"),
                      TestCase("showGridViewDownloads").InGuestMode(),
                      TestCase("showGridViewDrive").DisableDriveFs(),
                      TestCase("showGridViewDrive").EnableDriveFs(),
                      TestCase("showGridViewButtonSwitches"),
                      TestCase("showGridViewKeyboardSelectionA11y"),
                      TestCase("showGridViewMouseSelectionA11y")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Providers, /* providers.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("requestMount"),
                      TestCase("requestMount").DisableNativeSmb(),
                      TestCase("requestMountMultipleMounts"),
                      TestCase("requestMountMultipleMounts").DisableNativeSmb(),
                      TestCase("requestMountSourceDevice"),
                      TestCase("requestMountSourceDevice").DisableNativeSmb(),
                      TestCase("requestMountSourceFile"),
                      TestCase("requestMountSourceFile").DisableNativeSmb(),
                      TestCase("providerEject"),
                      TestCase("providerEject").DisableNativeSmb(),
                      TestCase("installNewServiceOnline"),
                      TestCase("installNewServiceOffline").Offline()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GearMenu, /* gear_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("showHiddenFilesDownloads"),
        TestCase("showHiddenFilesDownloads").InGuestMode(),
        TestCase("showHiddenFilesDrive").DisableDriveFs(),
        TestCase("showHiddenFilesDrive").EnableDriveFs(),
        TestCase("showPasteIntoCurrentFolder"),
        TestCase("showSelectAllInCurrentFolder"),
        TestCase("showToggleHiddenAndroidFoldersGearMenuItemsInMyFiles"),
        TestCase("enableToggleHiddenAndroidFoldersShowsHiddenFiles"),
        TestCase("hideCurrentDirectoryByTogglingHiddenAndroidFolders"),
        TestCase("newFolderInDownloads"),
        TestCase("showSendFeedbackAction")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FilesTooltip, /* files_tooltip.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("filesTooltipFocus"),
                      TestCase("filesTooltipMouseOver"),
                      TestCase("filesTooltipClickHides")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileList, /* file_list.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("fileListAriaAttributes"),
                      TestCase("fileListFocusFirstItem"),
                      TestCase("fileListSelectLastFocusedItem"),
                      TestCase("fileListKeyboardSelectionA11y"),
                      TestCase("fileListMouseSelectionA11y")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Crostini, /* crostini.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("mountCrostini"),
                      TestCase("enableDisableCrostini"),
                      TestCase("sharePathWithCrostini")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    MyFiles, /* my_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("directoryTreeRefresh"),
                      TestCase("showMyFiles"),
                      TestCase("myFilesDisplaysAndOpensEntries"),
                      TestCase("myFilesFolderRename"),
                      TestCase("myFilesUpdatesWhenAndroidVolumeMounts")

                          .DontMountVolumes(),
                      TestCase("myFilesUpdatesChildren"),
                      TestCase("myFilesAutoExpandOnce"),
                      TestCase("myFilesToolbarDelete")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    InstallLinuxPackageDialog, /* install_linux_package_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("installLinuxPackageDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    LauncherSearch, /* launcher_search.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("launcherOpenSearchResult"),
                      TestCase("launcherSearch"),
                      TestCase("launcherSearchOffline").Offline()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Recents, /* recents.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("recentsDownloads"),
        TestCase("recentsDrive").DisableDriveFs(),
        TestCase("recentsDrive").EnableDriveFs(),
        TestCase("recentsCrostiniNotMounted"),
        TestCase("recentsCrostiniMounted"),
        TestCase("recentsDownloadsAndDrive").DisableDriveFs(),
        TestCase("recentsDownloadsAndDrive").EnableDriveFs(),
        TestCase("recentsDownloadsAndDriveWithOverlap").DisableDriveFs(),
        TestCase("recentsDownloadsAndDriveWithOverlap").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metadata, /* metadata.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("metadataDocumentsProvider").EnableDocumentsProvider(),
        TestCase("metadataDownloads"),
        TestCase("metadataDrive").DisableDriveFs(),
        TestCase("metadataDrive").EnableDriveFs(),
        TestCase("metadataTeamDrives").DisableDriveFs(),
        TestCase("metadataTeamDrives").EnableDriveFs(),
        TestCase("metadataLargeDrive").DisableDriveFs(),
        TestCase("metadataLargeDrive").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Search, /* search.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("searchDownloadsWithResults"),
                      TestCase("searchDownloadsWithNoResults"),
                      TestCase("searchDownloadsClearSearch")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metrics, /* metrics.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("metricsRecordEnum")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Breadcrumbs, /* breadcrumbs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("breadcrumbsNavigate"),
                      TestCase("breadcrumbsLeafNoFocus"),
                      TestCase("breadcrumbsDownloadsTranslation")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FormatDialog, /* format_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("formatDialog").EnableFormatDialog(),
                      TestCase("formatDialogEmpty").EnableFormatDialog(),
                      TestCase("formatDialogCancel").EnableFormatDialog(),
                      TestCase("formatDialogNameLength").EnableFormatDialog(),
                      TestCase("formatDialogNameInvalid").EnableFormatDialog(),
                      TestCase("formatDialogGearMenu").EnableFormatDialog()));

// Structure to describe an account info.
struct TestAccountInfo {
  const char* const gaia_id;
  const char* const email;
  const char* const hash;
  const char* const display_name;
};

enum {
  DUMMY_ACCOUNT_INDEX = 0,
  PRIMARY_ACCOUNT_INDEX = 1,
  SECONDARY_ACCOUNT_INDEX_START = 2,
};

static const TestAccountInfo kTestAccounts[] = {
    {"gaia-id-d", "__dummy__@invalid.domain", "hashdummy", "Dummy Account"},
    {"gaia-id-a", "alice@invalid.domain", "hashalice", "Alice"},
    {"gaia-id-b", "bob@invalid.domain", "hashbob", "Bob"},
    {"gaia-id-c", "charlie@invalid.domain", "hashcharlie", "Charlie"},
};

// Test fixture class for testing multi-profile features.
class MultiProfileFilesAppBrowserTest : public FileManagerBrowserTestBase {
 public:
  MultiProfileFilesAppBrowserTest() = default;

 protected:
  // Enables multi-profiles.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileManagerBrowserTestBase::SetUpCommandLine(command_line);
    // Logs in to a dummy profile (For making MultiProfileWindowManager happy;
    // browser test creates a default window and the manager tries to assign a
    // user for it, and we need a profile connected to a user.)
    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    kTestAccounts[DUMMY_ACCOUNT_INDEX].email);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    kTestAccounts[DUMMY_ACCOUNT_INDEX].hash);
    // Don't require policy for our sessions - this is required because
    // this test creates a secondary profile synchronously, so we need to
    // let the policy code know not to expect cached policy.
    command_line->AppendSwitchASCII(chromeos::switches::kProfileRequiresPolicy,
                                    "false");
  }

  // Logs in to the primary profile of this test.
  void SetUpOnMainThread() override {
    const TestAccountInfo& info = kTestAccounts[PRIMARY_ACCOUNT_INDEX];

    AddUser(info, true);
    FileManagerBrowserTestBase::SetUpOnMainThread();
  }

  // Loads all users to the current session and sets up necessary fields.
  // This is used for preparing all accounts in PRE_ test setup, and for testing
  // actual login behavior.
  void AddAllUsers() {
    for (size_t i = 0; i < base::size(kTestAccounts); ++i) {
      // The primary account was already set up in SetUpOnMainThread, so skip it
      // here.
      if (i == PRIMARY_ACCOUNT_INDEX)
        continue;
      AddUser(kTestAccounts[i], i >= SECONDARY_ACCOUNT_INDEX_START);
    }
  }

  // Returns primary profile (if it is already created.)
  Profile* profile() override {
    Profile* const profile =
        chromeos::ProfileHelper::GetProfileByUserIdHashForTest(
            kTestAccounts[PRIMARY_ACCOUNT_INDEX].hash);
    return profile ? profile : FileManagerBrowserTestBase::profile();
  }

  // Adds a new user for testing to the current session.
  void AddUser(const TestAccountInfo& info, bool log_in) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(info.email, info.gaia_id));
    user_manager::User* user =
        user_manager::User::CreateRegularUserForTesting(account_id);
    static_cast<user_manager::UserManagerBase*>(
        user_manager::UserManager::Get())
        ->AddUserRecordForTesting(user);
    if (log_in) {
      session_manager::SessionManager::Get()->CreateSession(account_id,
                                                            info.hash, false);
    }
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, profile());
    user_manager::UserManager::Get()->SaveUserDisplayName(
        account_id, base::UTF8ToUTF16(info.display_name));
    Profile* profile =
        chromeos::ProfileHelper::GetProfileByUserIdHashForTest(info.hash);
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    if (!identity_manager->HasPrimaryAccount())
      signin::MakePrimaryAccountAvailable(identity_manager, info.email);
  }

  GuestMode GetGuestMode() const override { return NOT_IN_GUEST_MODE; }

  bool GetEnableDriveFs() const override { return false; }

  const char* GetTestCaseName() const override {
    return test_case_name_.c_str();
  }

  std::string GetFullTestCaseName() const override { return test_case_name_; }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

 private:
  std::string test_case_name_;

  DISALLOW_COPY_AND_ASSIGN(MultiProfileFilesAppBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MultiProfileFilesAppBrowserTest, PRE_BasicDownloads) {
  AddAllUsers();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFilesAppBrowserTest, BasicDownloads) {
  AddAllUsers();
  // Sanity check that normal operations work in multi-profile.
  set_test_case_name("keyboardCopyDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFilesAppBrowserTest, PRE_BasicDrive) {
  AddAllUsers();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFilesAppBrowserTest, BasicDrive) {
  AddAllUsers();
  // Sanity check that normal operations work in multi-profile.
  set_test_case_name("keyboardCopyDrive");
  StartTest();
}

// Test fixture class for testing migration to DriveFS.
class DriveFsFilesAppBrowserTest : public FileManagerBrowserTestBase {
 public:
  DriveFsFilesAppBrowserTest() = default;

 protected:
  GuestMode GetGuestMode() const override { return NOT_IN_GUEST_MODE; }

  const char* GetTestCaseName() const override {
    return test_case_name_.c_str();
  }

  std::string GetFullTestCaseName() const override { return test_case_name_; }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

  bool GetEnableDriveFs() const override {
    return !base::StringPiece(
                ::testing::UnitTest::GetInstance()->current_test_info()->name())
                .starts_with("PRE");
  }

  base::FilePath GetDriveDataDirectory() {
    return profile()->GetPath().Append("drive/v1");
  }

 private:
  std::string test_case_name_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsFilesAppBrowserTest);
};

IN_PROC_BROWSER_TEST_F(DriveFsFilesAppBrowserTest, PRE_MigratePinnedFiles) {
  set_test_case_name("PRE_driveMigratePinnedFile");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(DriveFsFilesAppBrowserTest, MigratePinnedFiles) {
  set_test_case_name("driveMigratePinnedFile");
  StartTest();

  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::IsDirectoryEmpty(GetDriveDataDirectory()));
}

IN_PROC_BROWSER_TEST_F(DriveFsFilesAppBrowserTest, PRE_RecoverDirtyFiles) {
  set_test_case_name("PRE_driveRecoverDirtyFiles");
  StartTest();

  {
    base::ScopedAllowBlockingForTesting allow_io;

    // Create a non-dirty file in the cache.
    base::WriteFile(GetDriveDataDirectory().Append("files/foo"), "data", 4);
  }
}

IN_PROC_BROWSER_TEST_F(DriveFsFilesAppBrowserTest, RecoverDirtyFiles) {
  set_test_case_name("driveRecoverDirtyFiles");
  StartTest();

  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::IsDirectoryEmpty(GetDriveDataDirectory()));
}

IN_PROC_BROWSER_TEST_F(DriveFsFilesAppBrowserTest, LaunchWithoutOldDriveData) {
  base::ScopedAllowBlockingForTesting allow_io;

  // After starting up, GCache/v1 should still be empty.
  EXPECT_TRUE(base::IsDirectoryEmpty(GetDriveDataDirectory()));
}

}  // namespace file_manager
