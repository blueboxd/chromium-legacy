// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_controller.h"

#include <queue>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_menu_model_adapter.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

namespace {

base::string16 GetLabelForClipboardData(const ui::ClipboardData& item) {
  if (item.format() & static_cast<int>(ui::ClipboardInternalFormat::kBitmap)) {
    return ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
        IDS_CLIPBOARD_MENU_IMAGE);
  }
  if (item.format() & static_cast<int>(ui::ClipboardInternalFormat::kText))
    return base::UTF8ToUTF16(item.text());
  if (item.format() & static_cast<int>(ui::ClipboardInternalFormat::kHtml))
    return base::UTF8ToUTF16(item.markup_data());
  if (item.format() & static_cast<int>(ui::ClipboardInternalFormat::kRtf)) {
    return ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
        IDS_CLIPBOARD_MENU_RTF_CONTENT);
  }
  if (item.format() &
      static_cast<int>(ui::ClipboardInternalFormat::kBookmark)) {
    return base::UTF8ToUTF16(item.bookmark_title());
  }
  if (item.format() & static_cast<int>(ui::ClipboardInternalFormat::kWeb)) {
    return ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
        IDS_CLIPBOARD_MENU_WEB_SMART_PASTE);
  }
  return base::string16();
}

ui::ImageModel GetImageModelForClipboardData(const ui::ClipboardData& item) {
  if (item.format() & static_cast<int>(ui::ClipboardInternalFormat::kBitmap)) {
    // TODO(newcomer): Show a smaller version of the bitmap.
    return ui::ImageModel();
  }
  if (item.format() & static_cast<int>(ui::ClipboardInternalFormat::kWeb))
    return ui::ImageModel::FromVectorIcon(ash::kWebSmartPasteIcon);
  if (item.format() &
      static_cast<int>(ui::ClipboardInternalFormat::kBookmark)) {
    return ui::ImageModel::FromVectorIcon(ash::kWebBookmarkIcon);
  }
  if (item.format() & static_cast<int>(ui::ClipboardInternalFormat::kHtml))
    return ui::ImageModel::FromVectorIcon(ash::kHtmlIcon);
  if (item.format() & static_cast<int>(ui::ClipboardInternalFormat::kRtf))
    return ui::ImageModel::FromVectorIcon(ash::kRtfIcon);
  if (item.format() & static_cast<int>(ui::ClipboardInternalFormat::kText))
    return ui::ImageModel::FromVectorIcon(ash::kTextIcon);
  // TODO(newcomer): Handle custom data types, which could be files.
  return ui::ImageModel();
}

void WriteClipboardDataToClipboard(const ui::ClipboardData& data) {
  ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
  if (data.format() & static_cast<int>(ui::ClipboardInternalFormat::kBitmap))
    writer.WriteImage(data.bitmap());
  if (data.format() & static_cast<int>(ui::ClipboardInternalFormat::kText))
    writer.WriteText(base::UTF8ToUTF16(data.text()));
  if (data.format() & static_cast<int>(ui::ClipboardInternalFormat::kHtml))
    writer.WriteHTML(base::UTF8ToUTF16(data.markup_data()), data.url());
  if (data.format() & static_cast<int>(ui::ClipboardInternalFormat::kRtf))
    writer.WriteRTF(data.rtf_data());
  if (data.format() &
      static_cast<int>(ui::ClipboardInternalFormat::kBookmark)) {
    writer.WriteBookmark(base::UTF8ToUTF16(data.bookmark_title()),
                         data.bookmark_url());
  }
  // TODO(newcomer): Handle custom data types, which could be files.
}

class ClipboardHistoryMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  ClipboardHistoryMenuDelegate(ClipboardHistoryController* controller)
      : controller_(controller) {}
  ClipboardHistoryMenuDelegate(const ClipboardHistoryMenuDelegate&) = delete;
  ClipboardHistoryMenuDelegate& operator=(const ClipboardHistoryMenuDelegate&) =
      delete;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    controller_->MenuOptionSelected(/*index=*/command_id);
  }

 private:
  // The controller responsible for showing the Clipboard History menu.
  ClipboardHistoryController* const controller_;
};

}  // namespace

class ClipboardHistoryAcceleratorTarget : public ui::AcceleratorTarget {
 public:
  ClipboardHistoryAcceleratorTarget(ClipboardHistoryController* controller)
      : controller_(controller) {}
  ClipboardHistoryAcceleratorTarget(const ClipboardHistoryAcceleratorTarget&) =
      delete;
  ClipboardHistoryAcceleratorTarget& operator=(
      const ClipboardHistoryAcceleratorTarget&) = delete;
  ~ClipboardHistoryAcceleratorTarget() override = default;

  void Init() {
    ui::Accelerator show_menu_combo(ui::VKEY_V, ui::EF_COMMAND_DOWN);
    show_menu_combo.set_key_state(ui::Accelerator::KeyState::PRESSED);
    // Register, but no need to unregister because this outlives
    // AcceleratorController.
    Shell::Get()->accelerator_controller()->Register(
        {show_menu_combo}, /*accelerator_target=*/this);
  }

 private:
  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    controller_->ShowMenu();
    return true;
  }

  bool CanHandleAccelerators() const override {
    return controller_->CanShowMenu();
  }

  // The controller responsible for showing the Clipboard History menu.
  ClipboardHistoryController* const controller_;
};

ClipboardHistoryController::ClipboardHistoryController()
    : clipboard_history_(std::make_unique<ClipboardHistory>()),
      accelerator_target_(
          std::make_unique<ClipboardHistoryAcceleratorTarget>(this)),
      menu_delegate_(std::make_unique<ClipboardHistoryMenuDelegate>(this)) {}

ClipboardHistoryController::~ClipboardHistoryController() = default;

void ClipboardHistoryController::Init() {
  accelerator_target_->Init();
}

bool ClipboardHistoryController::CanShowMenu() const {
  return !clipboard_history_->IsEmpty();
}

void ClipboardHistoryController::ShowMenu() {
  auto* host = ash::GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  const gfx::Rect textfield_bounds =
      host->GetInputMethod()->GetTextInputClient()->GetCaretBounds();
  if (textfield_bounds.IsEmpty()) {
    // Some web apps render the caret in an IFrame, and we will not get the
    // bounds in that case.
    // TODO(https://crbug.com/1099930): Show the menu in the middle of the
    // webview if the bounds are empty.
    return;
  }

  if (!CanShowMenu())
    return;

  clipboard_items_ =
      clipboard_history_->GetRecentClipboardDataWithNoDuplicates();

  std::unique_ptr<ui::SimpleMenuModel> menu_model =
      std::make_unique<ui::SimpleMenuModel>(menu_delegate_.get());
  menu_model->AddTitle(
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_CLIPBOARD_MENU_CLIPBOARD));
  int index = 0;
  for (const auto& item : clipboard_items_) {
    menu_model->AddItemWithIcon(index++, GetLabelForClipboardData(item),
                                GetImageModelForClipboardData(item));
  }
  menu_model->AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
  menu_model->AddItemWithIcon(
      index++,
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_CLIPBOARD_MENU_DELETE_ALL),
      ui::ImageModel::FromVectorIcon(ash::kDeleteIcon));

  context_menu_ =
      std::make_unique<ClipboardHistoryMenuModelAdapter>(std::move(menu_model));
  context_menu_->Run(textfield_bounds);
}

void ClipboardHistoryController::MenuOptionSelected(int index) {
  auto it = clipboard_items_.begin();
  std::advance(it, index);

  if (it == clipboard_items_.end()) {
    // The last option in the menu is used to delete history.
    clipboard_history_->ClearHistory();
    return;
  }

  // Pause clipboard history when manipulating the clipboard for the purpose of
  // a paste.
  ClipboardHistory::ScopedPause scoped_pause(clipboard_history_.get());

  // Place the selected item on top of the clipboard.
  const bool selected_item_not_on_top = it != clipboard_items_.begin();
  if (selected_item_not_on_top)
    WriteClipboardDataToClipboard(*it);

  ui::KeyEvent synthetic_key_event(ui::ET_KEY_PRESSED, ui::VKEY_V,
                                   static_cast<ui::DomCode>(0),
                                   ui::EF_CONTROL_DOWN);
  auto* host = ash::GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(host);
  host->DeliverEventToSink(&synthetic_key_event);

  // Replace the original item back on top of the clipboard.
  if (selected_item_not_on_top)
    WriteClipboardDataToClipboard(*(clipboard_items_.begin()));
}

}  // namespace ash