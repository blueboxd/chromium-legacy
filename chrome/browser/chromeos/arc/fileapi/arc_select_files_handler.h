// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "components/arc/common/file_system.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class SelectFileDialogHolder;

// Exposed for testing.
extern const char kScriptClickOk[];
extern const char kScriptClickCancel[];
extern const char kScriptClickDirectory[];
extern const char kScriptClickFile[];
extern const char kScriptGetElements[];

// Handler for FileSystemHost.SelectFiles.
class ArcSelectFilesHandler : public ui::SelectFileDialog::Listener {
 public:
  explicit ArcSelectFilesHandler(content::BrowserContext* context);
  ~ArcSelectFilesHandler() override;

  void SelectFiles(const mojom::SelectFilesRequestPtr& request,
                   mojom::FileSystemHost::SelectFilesCallback callback);

  void OnFileSelectorEvent(
      mojom::FileSelectorEventPtr event,
      mojom::FileSystemHost::OnFileSelectorEventCallback callback);

  void GetFileSelectorElements(
      mojom::GetFileSelectorElementsRequestPtr request,
      mojom::FileSystemHost::GetFileSelectorElementsCallback callback);

  // ui::SelectFileDialog::Listener overrides:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  friend class ArcSelectFilesHandlerTest;

  void FilesSelectedInternal(const std::vector<base::FilePath>& files,
                             void* params);

  void SetDialogHolderForTesting(
      std::unique_ptr<SelectFileDialogHolder> dialog_holder);

  Profile* const profile_;

  mojom::FileSystemHost::SelectFilesCallback callback_;
  std::unique_ptr<SelectFileDialogHolder> dialog_holder_;

  DISALLOW_COPY_AND_ASSIGN(ArcSelectFilesHandler);
};

// Wrapper for SelectFileDialogExtension.
// Since it is not easy to create a mock class for SelectFileDialogExtension,
// this class is replaced with a mock class instead in unit tests.
class SelectFileDialogHolder {
 public:
  explicit SelectFileDialogHolder(ui::SelectFileDialog::Listener* listener);
  virtual ~SelectFileDialogHolder();

  virtual void SelectFile(ui::SelectFileDialog::Type type,
                          const base::FilePath& default_path,
                          const ui::SelectFileDialog::FileTypeInfo* file_types,
                          int task_id,
                          bool show_android_picker_apps);

  virtual void ExecuteJavaScript(
      const std::string& script,
      content::RenderFrameHost::JavaScriptResultCallback callback);

 private:
  scoped_refptr<SelectFileDialogExtension> select_file_dialog_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_
