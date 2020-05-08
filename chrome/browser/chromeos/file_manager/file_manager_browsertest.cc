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
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_base.h"
#include "content/public/test/browser_test.h"

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

  TestCase& EnableDocumentsProvider() {
    enable_arc = true;
    enable_documents_provider.emplace(true);
    return *this;
  }

  TestCase& DisableDocumentsProvider() {
    enable_documents_provider.emplace(false);
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
    files_ng.emplace(true);
    return *this;
  }

  TestCase& DisableFilesNg() {
    files_ng.emplace(false);
    return *this;
  }

  TestCase& DisableNativeSmb() {
    enable_native_smb = false;
    return *this;
  }

  TestCase& EnableSmbfs() {
    enable_smbfs = true;
    return *this;
  }

  TestCase& EnableUnifiedMediaView() {
    enable_unified_media_view.emplace(true);
    return *this;
  }

  TestCase& DontMountVolumes() {
    mount_no_volumes = true;
    return *this;
  }

  TestCase& DontObserveFileTasks() {
    observe_file_tasks = false;
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

    if (!test.files_ng.value_or(true))
      name.append("_DisableFilesNg");

    if (!test.enable_native_smb)
      name.append("_DisableNativeSmb");

    if (test.enable_documents_provider.value_or(false))
      name.append("_DocumentsProvider");

    return name;
  }

  const char* test_case_name = nullptr;
  GuestMode guest_mode = NOT_IN_GUEST_MODE;
  bool tablet_mode = false;
  base::Optional<bool> enable_documents_provider;
  bool enable_arc = false;
  bool with_browser = false;
  bool needs_zip = false;
  bool offline = false;
  base::Optional<bool> files_ng;
  bool enable_native_smb = true;
  bool enable_smbfs = false;
  base::Optional<bool> enable_unified_media_view;
  bool mount_no_volumes = false;
  bool observe_file_tasks = true;
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

  bool GetEnableDocumentsProvider() const override {
    return GetParam().enable_documents_provider.value_or(
        FileManagerBrowserTestBase::GetEnableDocumentsProvider());
  }

  bool GetEnableArc() const override { return GetParam().enable_arc; }

  bool GetRequiresStartupBrowser() const override {
    return GetParam().with_browser;
  }

  bool GetNeedsZipSupport() const override { return GetParam().needs_zip; }

  bool GetIsOffline() const override { return GetParam().offline; }

  bool GetEnableFilesNg() const override {
    return GetParam().files_ng.value_or(
        FileManagerBrowserTestBase::GetEnableFilesNg());
  }

  bool GetEnableNativeSmb() const override {
    return GetParam().enable_native_smb;
  }

  bool GetEnableSmbfs() const override { return GetParam().enable_smbfs; }

  bool GetEnableUnifiedMediaView() const override {
    return GetParam().enable_unified_media_view.value_or(
        FileManagerBrowserTestBase::GetEnableUnifiedMediaView());
  }

  bool GetStartWithNoVolumesMounted() const override {
    return GetParam().mount_no_volumes;
  }

  bool GetStartWithFileTasksObserver() const override {
    return GetParam().observe_file_tasks;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FilesAppBrowserTest);
};

IN_PROC_BROWSER_TEST_P(FilesAppBrowserTest, Test) {
  StartTest();
}

// A version of the FilesAppBrowserTest that supports spanning browser restart
// to allow testing prefs and other things.
class ExtendedFilesAppBrowserTest : public FilesAppBrowserTest {
 public:
  ExtendedFilesAppBrowserTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtendedFilesAppBrowserTest);
};

IN_PROC_BROWSER_TEST_P(ExtendedFilesAppBrowserTest, PRE_Test) {
  profile()->GetPrefs()->SetBoolean(prefs::kNetworkFileSharesAllowed,
                                    GetEnableNativeSmb());
}

IN_PROC_BROWSER_TEST_P(ExtendedFilesAppBrowserTest, Test) {
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
        TestCase("fileDisplayLaunchOnLocalFolder").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnDrive").DontObserveFileTasks(),
        TestCase("fileDisplayDrive").TabletMode(),
        TestCase("fileDisplayDrive"),
        TestCase("fileDisplayDriveOffline").Offline(),
        TestCase("fileDisplayDriveOnline"),
        TestCase("fileDisplayComputers"),
        TestCase("fileDisplayMtp"),
        TestCase("fileDisplayUsb"),
        TestCase("fileDisplayUsbPartition"),
        TestCase("fileDisplayUsbPartitionSort"),
        TestCase("fileDisplayPartitionFileTable"),
        TestCase("fileSearch"),
        TestCase("fileDisplayWithoutDownloadsVolume").DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumes").DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumesThenMountDownloads")
            .DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumesThenMountDrive").DontMountVolumes(),
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
        TestCase("fileDisplayCheckNoReadOnlyIconOnDownloads"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnLinuxFiles"),
        TestCase("fileDisplayStartupError")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenVideoFiles, /* open_video_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("videoOpenDownloads").InGuestMode(),
                      TestCase("videoOpenDownloads"),
                      TestCase("videoOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenAudioFiles, /* open_audio_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("audioOpenCloseDownloads"),
                      TestCase("audioOpenCloseDownloads").InGuestMode(),
                      TestCase("audioOpenCloseDrive"),
                      TestCase("audioOpenDownloads").InGuestMode(),
                      TestCase("audioOpenDownloads"),
                      TestCase("audioOpenDrive"),
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
                      TestCase("imageOpenDrive"),
                      TestCase("imageOpenGalleryOpenDownloads"),
                      TestCase("imageOpenGalleryOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenSniffedFiles, /* open_sniffed_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("pdfOpenDownloads"),
                      TestCase("pdfOpenDrive"),
                      TestCase("textOpenDownloads"),
                      TestCase("textOpenDrive")));

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
                      ZipCase("zipFileOpenDrive"),
                      ZipCase("zipFileOpenUsb"),
                      ZipCase("zipCreateFileDownloads").InGuestMode(),
                      ZipCase("zipCreateFileDownloads"),
                      ZipCase("zipCreateFileDrive"),
                      ZipCase("zipCreateFileUsb")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CreateNewFolder, /* create_new_folder.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("selectCreateFolderDownloads").InGuestMode(),
                      TestCase("selectCreateFolderDownloads").FilesNg(),
                      TestCase("selectCreateFolderDownloads").DisableFilesNg(),
                      TestCase("createFolderDownloads").InGuestMode(),
                      TestCase("createFolderDownloads"),
                      TestCase("createFolderNestedDownloads"),
                      TestCase("createFolderDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    KeyboardOperations, /* keyboard_operations.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("keyboardDeleteDownloads").InGuestMode(),
                      TestCase("keyboardDeleteDownloads").FilesNg(),
                      TestCase("keyboardDeleteDownloads").DisableFilesNg(),
                      TestCase("keyboardDeleteDrive"),
                      TestCase("keyboardDeleteFolderDownloads").InGuestMode(),
                      TestCase("keyboardDeleteFolderDownloads"),
                      TestCase("keyboardDeleteFolderDrive"),
                      TestCase("keyboardCopyDownloads").InGuestMode(),
                      TestCase("keyboardCopyDownloads"),
                      TestCase("keyboardCopyDrive"),
                      TestCase("keyboardFocusOutlineVisible"),
                      TestCase("keyboardFocusOutlineVisibleMouse"),
                      TestCase("keyboardSelectDriveDirectoryTree"),
                      TestCase("keyboardDisableCopyWhenDialogDisplayed"),
                      TestCase("keyboardOpenNewWindow"),
                      TestCase("keyboardOpenNewWindow").InGuestMode(),
                      TestCase("renameFileDownloads").InGuestMode(),
                      TestCase("renameFileDownloads"),
                      TestCase("renameFileDrive"),
                      TestCase("renameNewFolderDownloads").InGuestMode(),
                      TestCase("renameNewFolderDownloads").FilesNg(),
                      TestCase("renameNewFolderDownloads").DisableFilesNg(),
                      TestCase("renameNewFolderDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ContextMenu, /* context_menu.js for file list */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("checkDeleteEnabledForReadWriteFile"),
        TestCase("checkDeleteDisabledForReadOnlyDocument"),
        TestCase("checkDeleteDisabledForReadOnlyFile"),
        TestCase("checkDeleteDisabledForReadOnlyFolder"),
        TestCase("checkRenameEnabledForReadWriteFile"),
        TestCase("checkRenameDisabledForReadOnlyDocument"),
        TestCase("checkRenameDisabledForReadOnlyFile"),
        TestCase("checkRenameDisabledForReadOnlyFolder"),
        TestCase("checkContextMenuForRenameInput"),
        TestCase("checkShareEnabledForReadWriteFile"),
        TestCase("checkShareEnabledForReadOnlyDocument"),
        TestCase("checkShareDisabledForStrictReadOnlyDocument"),
        TestCase("checkShareEnabledForReadOnlyFile"),
        TestCase("checkShareEnabledForReadOnlyFolder"),
        TestCase("checkCopyEnabledForReadWriteFile"),
        TestCase("checkCopyEnabledForReadOnlyDocument"),
        TestCase("checkCopyDisabledForStrictReadOnlyDocument"),
        TestCase("checkCopyEnabledForReadOnlyFile"),
        TestCase("checkCopyEnabledForReadOnlyFolder"),
        TestCase("checkCutEnabledForReadWriteFile"),
        TestCase("checkCutDisabledForReadOnlyDocument"),
        TestCase("checkCutDisabledForReadOnlyFile"),
        TestCase("checkCutDisabledForReadOnlyFolder"),
        TestCase("checkPasteIntoFolderEnabledForReadWriteFolder"),
        TestCase("checkPasteIntoFolderDisabledForReadOnlyFolder"),
        TestCase("checkInstallWithLinuxDisabledForDebianFile"),
        TestCase("checkInstallWithLinuxEnabledForDebianFile"),
        TestCase("checkImportCrostiniImageEnabled"),
        TestCase("checkImportCrostiniImageDisabled"),
        TestCase("checkNewFolderEnabledInsideReadWriteFolder"),
        TestCase("checkNewFolderDisabledInsideReadOnlyFolder"),
        TestCase("checkPasteEnabledInsideReadWriteFolder"),
        TestCase("checkPasteDisabledInsideReadOnlyFolder"),
        TestCase("checkDownloadsContextMenu"),
        TestCase("checkPlayFilesContextMenu"),
        TestCase("checkLinuxFilesContextMenu"),
        TestCase("checkDeleteDisabledInDocProvider").EnableDocumentsProvider(),
        TestCase("checkDeleteEnabledInDocProvider").EnableDocumentsProvider(),
        TestCase("checkRenameDisabledInDocProvider").EnableDocumentsProvider(),
        TestCase("checkRenameEnabledInDocProvider").EnableDocumentsProvider(),
        TestCase("checkContextMenuFocus"),
        TestCase("checkContextMenusForInputElements")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Toolbar, /* toolbar.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("toolbarDeleteWithMenuItemNoEntrySelected"),
                      TestCase("toolbarDeleteButtonKeepFocus"),
                      TestCase("toolbarDeleteEntry").InGuestMode(),
                      TestCase("toolbarDeleteEntry"),
                      TestCase("toolbarRefreshButtonWithSelection").EnableArc(),
                      TestCase("toolbarAltACommand").FilesNg(),
                      TestCase("toolbarRefreshButtonHiddenInRecents"),
                      TestCase("toolbarMultiMenuFollowsButton")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    QuickView, /* quick_view.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openQuickView").DisableFilesNg(),
        TestCase("openQuickView").FilesNg(),
        TestCase("openQuickViewDialog"),
        TestCase("openQuickViewAndEscape"),
        TestCase("openQuickView").InGuestMode(),
        TestCase("openQuickView").TabletMode(),
        TestCase("openQuickViewViaContextMenuSingleSelection"),
        TestCase("openQuickViewViaContextMenuCheckSelections"),
        TestCase("openQuickViewAudio"),
        TestCase("openQuickViewAudioOnDrive"),
        TestCase("openQuickViewAudioWithImageMetadata"),
        TestCase("openQuickViewImageJpg"),
        TestCase("openQuickViewImageJpeg"),
        TestCase("openQuickViewImageExif"),
        TestCase("openQuickViewImageRaw"),
        TestCase("openQuickViewImageRawWithOrientation"),
        TestCase("openQuickViewBrokenImage"),
        TestCase("openQuickViewImageClick"),
        TestCase("openQuickViewVideo"),
        TestCase("openQuickViewVideoOnDrive"),
        TestCase("openQuickViewPdf"),
        TestCase("openQuickViewPdfPreviewsDisabled"),
        TestCase("openQuickViewKeyboardUpDownChangesView"),
        TestCase("openQuickViewKeyboardLeftRightChangesView"),
        TestCase("openQuickViewSniffedText"),
        TestCase("openQuickViewTextFileWithUnknownMimeType"),
        TestCase("openQuickViewScrollText"),
        TestCase("openQuickViewScrollHtml"),
        TestCase("openQuickViewMhtml"),
        TestCase("openQuickViewBackgroundColorHtml"),
        TestCase("openQuickViewDrive"),
        TestCase("openQuickViewSmbfs").EnableSmbfs(),
        TestCase("openQuickViewAndroid"),
        TestCase("openQuickViewDocumentsProvider").EnableDocumentsProvider(),
        TestCase("openQuickViewCrostini"),
        TestCase("openQuickViewUsb"),
        TestCase("openQuickViewRemovablePartitions"),
        TestCase("openQuickViewMtp"),
        TestCase("openQuickViewTabIndexImage"),
        TestCase("openQuickViewTabIndexText"),
        TestCase("openQuickViewTabIndexAudio"),
        TestCase("openQuickViewTabIndexVideo"),
        TestCase("openQuickViewTabIndexDeleteDialog"),
        TestCase("openQuickViewToggleInfoButtonKeyboard"),
        TestCase("openQuickViewToggleInfoButtonClick"),
        TestCase("openQuickViewWithMultipleFiles"),
        TestCase("openQuickViewWithMultipleFilesText"),
        TestCase("openQuickViewWithMultipleFilesPdf"),
        TestCase("openQuickViewWithMultipleFilesKeyboardUpDown"),
        TestCase("openQuickViewWithMultipleFilesKeyboardLeftRight"),
        TestCase("openQuickViewFromDirectoryTree"),
        TestCase("openQuickViewAndDeleteSingleSelection"),
        TestCase("openQuickViewAndDeleteCheckSelection"),
        TestCase("openQuickViewDeleteEntireCheckSelection"),
        TestCase("openQuickViewClickDeleteButton"),
        TestCase("openQuickViewDeleteButtonNotShown"),
        TestCase("openQuickViewUmaViaContextMenu"),
        TestCase("openQuickViewUmaForCheckSelectViaContextMenu"),
        TestCase("openQuickViewUmaViaSelectionMenu"),
        TestCase("openQuickViewUmaViaSelectionMenuKeyboard"),
        TestCase("closeQuickView")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTree, /* directory_tree.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeActiveDirectory").DisableFilesNg(),
        TestCase("directoryTreeActiveDirectory").FilesNg(),
        TestCase("directoryTreeSelectedDirectory").DisableFilesNg(),
        TestCase("directoryTreeSelectedDirectory").FilesNg(),
        TestCase("directoryTreeRecentsSubtypeScroll").EnableUnifiedMediaView(),
        TestCase("directoryTreeHorizontalScroll"),
        TestCase("directoryTreeExpandHorizontalScroll"),
        // Disabled. Fails on internal ChromeOS bot. https://crbug.com/1061821.
        // TestCase("directoryTreeExpandHorizontalScrollRTL"),
        TestCase("directoryTreeVerticalScroll"),
        TestCase("directoryTreeExpandFolder")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTreeContextMenu, /* directory_tree_context_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("dirCopyWithContextMenu").InGuestMode(),
        TestCase("dirCopyWithContextMenu").FilesNg(),
        TestCase("dirCopyWithContextMenu").DisableFilesNg(),
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
        TestCase("dirRenameToExisting"),
        TestCase("dirRenameRemovableWithKeyboard"),
        TestCase("dirRenameRemovableWithKeyboard").InGuestMode(),
        TestCase("dirRenameRemovableWithContentMenu"),
        TestCase("dirRenameRemovableWithContentMenu").InGuestMode(),
        TestCase("dirContextMenuForRenameInput"),
        TestCase("dirCreateWithContextMenu"),
        TestCase("dirCreateWithKeyboard"),
        TestCase("dirCreateWithoutChangingCurrent"),
        TestCase("dirCreateMultipleFolders"),
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
    ::testing::Values(TestCase("driveOpenSidebarOffline"),
                      TestCase("driveOpenSidebarSharedWithMe"),
                      TestCase("driveAutoCompleteQuery"),
                      TestCase("drivePinFileMobileNetwork"),
                      TestCase("driveClickFirstSearchResult"),
                      TestCase("drivePressEnterToSearch"),
                      TestCase("drivePressClearSearch"),
                      TestCase("drivePressCtrlAFromSearch"),
                      TestCase("driveBackupPhotos"),
                      TestCase("driveAvailableOfflineGearMenu"),
                      TestCase("driveAvailableOfflineDirectoryGearMenu"),
                      TestCase("driveLinkToDirectory"),
                      TestCase("driveLinkOpenFileThroughLinkedDirectory"),
                      TestCase("driveLinkOpenFileThroughTransitiveLink"),
                      TestCase("driveWelcomeBanner")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Transfer, /* transfer.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("transferFromDriveToDownloads"),
        TestCase("transferFromDownloadsToMyFiles"),
        TestCase("transferFromDownloadsToMyFilesMove"),
        TestCase("transferFromDownloadsToDrive"),
        TestCase("transferFromSharedToDownloads"),
        TestCase("transferFromSharedToDrive"),
        TestCase("transferFromOfflineToDownloads"),
        TestCase("transferFromOfflineToDrive"),
        TestCase("transferFromTeamDriveToDrive"),
        TestCase("transferFromDriveToTeamDrive"),
        TestCase("transferFromTeamDriveToDownloads"),
        TestCase("transferHostedFileFromTeamDriveToDownloads"),
        TestCase("transferFromDownloadsToTeamDrive").DisableFilesNg(),
        TestCase("transferFromDownloadsToTeamDrive").FilesNg(),
        TestCase("transferBetweenTeamDrives").DisableFilesNg(),
        TestCase("transferBetweenTeamDrives").FilesNg(),
        TestCase("transferDragAndDrop"),
        TestCase("transferDragAndHover"),
        TestCase("transferFromDownloadsToDownloads"),
        TestCase("transferDeletedFile"),
        TestCase("transferInfoIsRemembered"),
        TestCase("transferToUsbHasDestinationText"),
        TestCase("transferDismissedErrorIsRemembered")));

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
    ::testing::Values(TestCase("shareFileDrive").DisableFilesNg(),
                      TestCase("shareFileDrive").FilesNg(),
                      TestCase("shareDirectoryDrive"),
                      TestCase("shareHostedFileDrive"),
                      TestCase("manageHostedFileDrive"),
                      TestCase("manageFileDrive"),
                      TestCase("manageDirectoryDrive"),
                      TestCase("shareFileTeamDrive"),
                      TestCase("shareDirectoryTeamDrive"),
                      TestCase("shareHostedFileTeamDrive"),
                      TestCase("shareTeamDrive"),
                      TestCase("manageHostedFileTeamDrive"),
                      TestCase("manageFileTeamDrive"),
                      TestCase("manageDirectoryTeamDrive").DisableFilesNg(),
                      TestCase("manageDirectoryTeamDrive").FilesNg(),
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
                      TestCase("traverseDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Tasks, /* tasks.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("executeDefaultTaskDownloads"),
                      TestCase("executeDefaultTaskDownloads").InGuestMode(),
                      TestCase("executeDefaultTaskDrive"),
                      TestCase("defaultTaskForPdf"),
                      TestCase("defaultTaskForTextPlain"),
                      TestCase("defaultTaskDialogDownloads"),
                      TestCase("defaultTaskDialogDownloads").InGuestMode(),
                      TestCase("defaultTaskDialogDrive"),
                      TestCase("genericTaskIsNotExecuted"),
                      TestCase("genericTaskAndNonGenericTask")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FolderShortcuts, /* folder_shortcuts.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseFolderShortcuts").DisableFilesNg(),
                      TestCase("traverseFolderShortcuts").FilesNg(),
                      TestCase("addRemoveFolderShortcuts").DisableFilesNg(),
                      TestCase("addRemoveFolderShortcuts").FilesNg()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    SortColumns, /* sort_columns.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("sortColumns"),
                      TestCase("sortColumns").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    TabIndex, /* tab_index.js: */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("tabindexSearchBoxFocus").FilesNg(),
        TestCase("tabindexSearchBoxFocus").DisableFilesNg(),
        TestCase("tabindexFocus").DisableFilesNg(),
        TestCase("tabindexFocusDownloads").FilesNg(),
        TestCase("tabindexFocusDownloads").DisableFilesNg(),
        TestCase("tabindexFocusDownloads").InGuestMode().FilesNg(),
        TestCase("tabindexFocusDownloads").InGuestMode().DisableFilesNg(),
        // TestCase("tabindexFocusBreadcrumbBackground").FilesNg(),
        TestCase("tabindexFocusBreadcrumbBackground").DisableFilesNg(),
        TestCase("tabindexFocusDirectorySelected").FilesNg(),
        TestCase("tabindexFocusDirectorySelected").DisableFilesNg(),
        TestCase("tabindexOpenDialogDownloadsFilesNg").WithBrowser().FilesNg(),
        TestCase("tabindexOpenDialogDownloads").WithBrowser().DisableFilesNg(),
        TestCase("tabindexOpenDialogDownloads")
            .WithBrowser()
            .InGuestMode()
            .DisableFilesNg(),
        TestCase("tabindexOpenDialogDownloadsFilesNg")
            .WithBrowser()
            .InGuestMode()
            .FilesNg(),
        TestCase("tabindexSaveFileDialogDriveFilesNg").WithBrowser().FilesNg(),
        TestCase("tabindexSaveFileDialogDrive").WithBrowser().DisableFilesNg(),
        TestCase("tabindexSaveFileDialogDownloadsFilesNg")
            .WithBrowser()
            .FilesNg(),
        TestCase("tabindexSaveFileDialogDownloads")
            .WithBrowser()
            .DisableFilesNg(),
        TestCase("tabindexSaveFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()
            .DisableFilesNg(),
        TestCase("tabindexSaveFileDialogDownloadsFilesNg")
            .WithBrowser()
            .InGuestMode()
            .FilesNg()));

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
        TestCase("openFileDialogDrive").WithBrowser(),
        TestCase("openFileDialogDrive").WithBrowser().InIncognito(),
        TestCase("saveFileDialogDrive").WithBrowser(),
        TestCase("saveFileDialogDrive").WithBrowser().InIncognito(),
        TestCase("openFileDialogDriveFromBrowser").WithBrowser(),
        TestCase("openFileDialogDriveHostedDoc").WithBrowser(),
        TestCase("openFileDialogDriveHostedNeedsFile").WithBrowser(),
        TestCase("saveFileDialogDriveHostedNeedsFile").WithBrowser(),
        TestCase("openFileDialogCancelDrive").WithBrowser(),
        TestCase("openFileDialogEscapeDrive").WithBrowser(),
        TestCase("openFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("saveFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("openFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("saveFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("openFileDialogDefaultFilter").WithBrowser(),
        TestCase("saveFileDialogDefaultFilter").WithBrowser(),
        TestCase("openFileDialogFileListShowContextMenu").WithBrowser(),
        TestCase("openFileDialogSelectAllDisabled").WithBrowser(),
        TestCase("openMultiFileDialogSelectAllEnabled").WithBrowser()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CopyBetweenWindows, /* copy_between_windows.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("copyBetweenWindowsLocalToDrive"),
                      TestCase("copyBetweenWindowsLocalToUsb"),
                      TestCase("copyBetweenWindowsUsbToDrive"),
                      TestCase("copyBetweenWindowsDriveToLocal"),
                      TestCase("copyBetweenWindowsDriveToUsb"),
                      TestCase("copyBetweenWindowsUsbToLocal")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GridView, /* grid_view.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("showGridViewDownloads").DisableFilesNg(),
                      TestCase("showGridViewDownloads").InGuestMode(),
                      TestCase("showGridViewDownloads").FilesNg(),
                      TestCase("showGridViewDrive"),
                      TestCase("showGridViewButtonSwitches"),
                      TestCase("showGridViewKeyboardSelectionA11y"),
                      TestCase("showGridViewTitles").FilesNg(),
                      TestCase("showGridViewMouseSelectionA11y")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Providers, /* providers.js */
    ExtendedFilesAppBrowserTest,
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
        TestCase("showHiddenFilesDrive"),
        TestCase("showPasteIntoCurrentFolder"),
        TestCase("showSelectAllInCurrentFolder"),
        TestCase("showToggleHiddenAndroidFoldersGearMenuItemsInMyFiles"),
        TestCase("enableToggleHiddenAndroidFoldersShowsHiddenFiles"),
        TestCase("hideCurrentDirectoryByTogglingHiddenAndroidFolders"),
        TestCase("newFolderInDownloads"),
        TestCase("showSendFeedbackAction"),
        TestCase("enableDisableStorageSettingsLink")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FilesTooltip, /* files_tooltip.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("filesTooltipFocus"),
                      TestCase("filesTooltipMouseOver"),
                      TestCase("filesTooltipClickHides"),
                      TestCase("filesTooltipHidesOnWindowResize"),
                      TestCase("filesCardTooltipClickHides")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileList, /* file_list.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fileListAriaAttributes").DisableFilesNg(),
        TestCase("fileListAriaAttributes").FilesNg(),
        TestCase("fileListFocusFirstItem").DisableFilesNg(),
        TestCase("fileListFocusFirstItem").FilesNg(),
        TestCase("fileListSelectLastFocusedItem").DisableFilesNg(),
        TestCase("fileListSelectLastFocusedItem").FilesNg(),
        TestCase("fileListKeyboardSelectionA11y").DisableFilesNg(),
        TestCase("fileListKeyboardSelectionA11y").FilesNg(),
        TestCase("fileListMouseSelectionA11y").DisableFilesNg(),
        TestCase("fileListMouseSelectionA11y").FilesNg(),
        TestCase("fileListDeleteMultipleFiles").DisableFilesNg(),
        TestCase("fileListDeleteMultipleFiles").FilesNg(),
        TestCase("fileListRenameFromSelectAll").FilesNg()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Crostini, /* crostini.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("mountCrostini"),
                      TestCase("enableDisableCrostini"),
                      TestCase("sharePathWithCrostini"),
                      TestCase("pluginVmErrorDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    MyFiles, /* my_files.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeRefresh"),
        TestCase("showMyFiles").DisableFilesNg(),
        TestCase("myFilesDisplaysAndOpensEntries"),
        TestCase("myFilesFolderRename"),
        TestCase("myFilesUpdatesWhenAndroidVolumeMounts").DontMountVolumes(),
        TestCase("myFilesUpdatesChildren"),
        TestCase("myFilesAutoExpandOnce").DisableFilesNg(),
        TestCase("myFilesAutoExpandOnce").FilesNg(),
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
        TestCase("recentsDrive"),
        TestCase("recentsCrostiniNotMounted"),
        TestCase("recentsCrostiniMounted"),
        TestCase("recentsDownloadsAndDrive"),
        TestCase("recentsDownloadsAndDriveWithOverlap"),
        TestCase("recentAudioDownloads").EnableUnifiedMediaView(),
        TestCase("recentAudioDownloadsAndDrive").EnableUnifiedMediaView(),
        TestCase("recentImagesDownloads").EnableUnifiedMediaView(),
        TestCase("recentImagesDownloadsAndDrive").EnableUnifiedMediaView(),
        TestCase("recentVideosDownloads").EnableUnifiedMediaView(),
        TestCase("recentVideosDownloadsAndDrive").EnableUnifiedMediaView()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metadata, /* metadata.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("metadataDocumentsProvider").EnableDocumentsProvider(),
        TestCase("metadataDownloads").DisableFilesNg(),
        TestCase("metadataDownloads").FilesNg(),
        TestCase("metadataDrive").DisableFilesNg(),
        TestCase("metadataDrive").FilesNg(),
        TestCase("metadataTeamDrives"),
        TestCase("metadataLargeDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Search, /* search.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("searchDownloadsWithResults"),
                      TestCase("searchDownloadsWithNoResults"),
                      TestCase("searchDownloadsClearSearchKeyDown"),
                      TestCase("searchDownloadsClearSearch"),
                      TestCase("searchHidingViaTab"),
                      TestCase("searchHidingTextEntryField"),
                      TestCase("searchButtonToggles")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metrics, /* metrics.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("metricsRecordEnum")));

// TODO(adanilo) Remove 'breadcrumbsLeafNoFocus' when files-ng ships.
WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Breadcrumbs, /* breadcrumbs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("breadcrumbsNavigate").DisableFilesNg(),
                      TestCase("breadcrumbsLeafNoFocus").DisableFilesNg(),
                      TestCase("breadcrumbsTooltip").DisableFilesNg(),
                      TestCase("breadcrumbsDownloadsTranslation"),
                      TestCase("breadcrumbsRenderShortPath").FilesNg(),
                      TestCase("breadcrumbsEliderButtonHidden").FilesNg(),
                      TestCase("breadcrumbsRenderLongPath").FilesNg(),
                      TestCase("breadcrumbsMainButtonClick").FilesNg(),
                      TestCase("breadcrumbsMainButtonEnterKey").FilesNg(),
                      TestCase("breadcrumbsEliderButtonClick").FilesNg(),
                      TestCase("breadcrumbsEliderButtonKeyboard").FilesNg(),
                      TestCase("breadcrumbsEliderMenuClickOutside").FilesNg(),
                      TestCase("breadcrumbsEliderMenuItemClick").FilesNg(),
                      TestCase("breadcrumbsEliderMenuItemTabLeft").FilesNg(),
                      TestCase("breadcrumbsEliderMenuItemTabRight").FilesNg()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FormatDialog, /* format_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("formatDialog"),
                      TestCase("formatDialogEmpty"),
                      TestCase("formatDialogCancel"),
                      TestCase("formatDialogNameLength"),
                      TestCase("formatDialogNameInvalid"),
                      TestCase("formatDialogGearMenu")));

}  // namespace file_manager
