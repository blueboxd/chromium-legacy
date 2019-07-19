// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_jstest_base.h"

class FileManagerJsTest : public FileManagerJsTestBase {
 protected:
  FileManagerJsTest() : FileManagerJsTestBase(
      base::FilePath(FILE_PATH_LITERAL("ui/file_manager/file_manager"))) {}
};

// Tests that draw to canvases and test pixels need pixel output turned on.
class CanvasFileManagerJsTest : public FileManagerJsTest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    FileManagerJsTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ActionsSubmenuTest) {
  RunGeneratedTest("/foreground/js/ui/actions_submenu_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ActionsModelTest) {
  RunGeneratedTest("/foreground/js/actions_model_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, NavigationListModelTest) {
  RunGeneratedTest("/foreground/js/navigation_list_model_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileOperationHandlerTest) {
  RunGeneratedTest("/background/js/file_operation_handler_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ProgressCenterItemGroupTest) {
  RunGeneratedTest("/foreground/js/progress_center_item_group_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DeviceHandlerTest) {
  RunGeneratedTest("/background/js/device_handler_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileOperationManagerTest) {
  RunGeneratedTest("/background/js/file_operation_manager_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DriveSyncHandlerTest) {
  RunGeneratedTest("/background/js/drive_sync_handler_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImporterCommonTest) {
  RunGeneratedTest("/common/js/importer_common_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImportHistoryTest) {
  RunGeneratedTest("/background/js/import_history_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, VolumeManagerTest) {
  RunGeneratedTest("/background/js/volume_manager_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DirectoryTreeTest) {
  RunGeneratedTest("/foreground/js/ui/directory_tree_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTableTest) {
  RunGeneratedTest("/foreground/js/ui/file_table_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MediaScannerTest) {
  RunGeneratedTest("/background/js/media_scanner_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, LRUCacheTest) {
  RunGeneratedTest("/common/js/lru_cache_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MediaImportHandlerTest) {
  RunGeneratedTest("/background/js/media_import_handler_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MountMetricsTest) {
  RunGeneratedTest("/background/js/mount_metrics_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, TaskQueueTest) {
  RunGeneratedTest("/background/js/task_queue_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DuplicateFinderTest) {
  RunGeneratedTest("/background/js/duplicate_finder_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImportControllerTest) {
  RunGeneratedTest("/foreground/js/import_controller_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, TaskController) {
  RunGeneratedTest("/foreground/js/task_controller_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTasks) {
  RunGeneratedTest("/foreground/js/file_tasks_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ThumbnailLoader) {
  RunGeneratedTest("/foreground/js/thumbnail_loader_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataCacheItem) {
  RunGeneratedTest("/foreground/js/metadata/metadata_cache_item_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataCacheSet) {
  RunGeneratedTest("/foreground/js/metadata/metadata_cache_set_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MultiMetadataProvider) {
  RunGeneratedTest(
      "/foreground/js/metadata/multi_metadata_provider_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ListThumbnailLoader) {
  RunGeneratedTest("/foreground/js/list_thumbnail_loader_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileSystemMetadataProvider) {
  RunGeneratedTest(
      "/foreground/js/metadata/file_system_metadata_provider_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ExternalMetadataProvider) {
  RunGeneratedTest(
      "/foreground/js/metadata/external_metadata_provider_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ContentMetadataProvider) {
  RunGeneratedTest(
      "/foreground/js/metadata/content_metadata_provider_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataModel) {
  RunGeneratedTest("/foreground/js/metadata/metadata_model_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ThumbnailModel) {
  RunGeneratedTest("/foreground/js/metadata/thumbnail_model_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ExifParser) {
  RunGeneratedTest("/foreground/js/metadata/exif_parser_unittest.html");
}

IN_PROC_BROWSER_TEST_F(CanvasFileManagerJsTest, ImageOrientation) {
  RunGeneratedTest("/foreground/js/metadata/image_orientation_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ProvidersModel) {
  RunGeneratedTest("/foreground/js/providers_model_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesMessage) {
  RunGeneratedTest("/foreground/elements/files_message_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesToast) {
  RunGeneratedTest("/foreground/elements/files_toast_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, SpinnerController) {
  RunGeneratedTest("/foreground/js/spinner_controller_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileListModel) {
  RunGeneratedTest("/foreground/js/file_list_model_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTapHandler) {
  RunGeneratedTest("/foreground/js/ui/file_tap_handler_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileListSelectionModelTest) {
  RunGeneratedTest("/foreground/js/ui/file_list_selection_model_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MultiMenu) {
  RunGeneratedTest("/foreground/js/ui/multi_menu_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesAppEntryTypes) {
  RunGeneratedTest("/common/js/files_app_entry_types_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileType) {
  RunGeneratedTest("/common/js/file_type_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, UtilTest) {
  RunGeneratedTest("/common/js/util_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Crostini) {
  RunGeneratedTest("/background/js/crostini_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTableList) {
  RunGeneratedTest("/foreground/js/ui/file_table_list_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTransferController) {
  RunGeneratedTest("/foreground/js/file_transfer_controller_unittest.html");
}
