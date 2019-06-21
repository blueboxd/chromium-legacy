// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/seat.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#endif  // defined(OS_CHROMEOS)

#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/exo/data_source.h"
#include "components/exo/mime_utils.h"
#include "components/exo/seat_observer.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/icu/source/common/unicode/ucnv.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace exo {
namespace {

// The maximum number of different data types that we will write to the
// clipboard (plain text, RTF, HTML, image)
constexpr int kMaxClipboardDataTypes = 4;

constexpr char kUTF16Unspecified[] = "UTF-16";
constexpr char kUTF16LittleEndian[] = "UTF-16LE";
constexpr char kUTF16BigEndian[] = "UTF-16BE";
constexpr uint8_t kByteOrderMark[] = {0xFE, 0xFF};
constexpr int kByteOrderMarkSize = sizeof(kByteOrderMark);

Surface* GetEffectiveFocus(aura::Window* window) {
  if (!window)
    return nullptr;
  Surface* const surface = Surface::AsSurface(window);
  if (surface)
    return surface;
  // Fallback to main surface.
  aura::Window* const top_level_window = window->GetToplevelWindow();
  if (!top_level_window)
    return nullptr;
  return GetShellMainSurface(top_level_window);
}

base::string16 CodepageToUTF16(const std::vector<uint8_t>& data,
                               const std::string& charset_input) {
  base::string16 output;
  base::StringPiece piece(reinterpret_cast<const char*>(data.data()),
                          data.size());
  const char* charset = charset_input.c_str();

  // Despite claims in the documentation to the contrary, the ICU UTF-16
  // converter does not automatically detect and interpret the byte order
  // mark. Therefore, we must do this ourselves.
  if (!ucnv_compareNames(charset, kUTF16Unspecified) &&
      data.size() >= kByteOrderMarkSize) {
    if (static_cast<uint8_t>(piece.data()[0]) == kByteOrderMark[0] &&
        static_cast<uint8_t>(piece.data()[1]) == kByteOrderMark[1]) {
      // BOM is in big endian format. Consume the BOM so it doesn't get
      // interpreted as a character.
      piece.remove_prefix(2);
      charset = kUTF16BigEndian;
    } else if (static_cast<uint8_t>(piece.data()[0]) == kByteOrderMark[1] &&
               static_cast<uint8_t>(piece.data()[1]) == kByteOrderMark[0]) {
      // BOM is in little endian format. Consume the BOM so it doesn't get
      // interpreted as a character.
      piece.remove_prefix(2);
      charset = kUTF16LittleEndian;
    }
  }

  base::CodepageToUTF16(
      piece, charset, base::OnStringConversionError::Type::SUBSTITUTE, &output);
  return output;
}

}  // namespace

Seat::Seat()
    : changing_clipboard_data_to_selection_source_(false),
      weak_ptr_factory_(this) {
  WMHelper::GetInstance()->AddFocusObserver(this);
  // Prepend handler as it's critical that we see all events.
  WMHelper::GetInstance()->PrependPreTargetHandler(this);
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  // TODO(reveman): Need to handle the mus case where PlatformEventSource is
  // null. https://crbug.com/856230
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
}

Seat::~Seat() {
  DCHECK(!selection_source_) << "DataSource must be released before Seat";
  WMHelper::GetInstance()->RemoveFocusObserver(this);
  WMHelper::GetInstance()->RemovePreTargetHandler(this);
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
}

void Seat::AddObserver(SeatObserver* observer) {
  observers_.AddObserver(observer);
}

void Seat::RemoveObserver(SeatObserver* observer) {
  observers_.RemoveObserver(observer);
}

Surface* Seat::GetFocusedSurface() {
  return GetEffectiveFocus(WMHelper::GetInstance()->GetFocusedWindow());
}

void Seat::SetSelection(DataSource* source) {
  if (!source) {
    ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardType::kCopyPaste);
    // selection_source_ is Cancelled() and reset() in OnClipboardDataChanged().
    return;
  }

  if (selection_source_) {
    if (selection_source_->get() == source)
      return;
    selection_source_->get()->Cancelled();
  }
  selection_source_ = std::make_unique<ScopedDataSource>(source, this);
  scoped_refptr<RefCountedScopedClipboardWriter> writer =
      base::MakeRefCounted<RefCountedScopedClipboardWriter>(
          ui::ClipboardType::kCopyPaste);

  base::RepeatingClosure data_read_callback = base::BarrierClosure(
      kMaxClipboardDataTypes,
      base::BindOnce(&Seat::OnAllReadsFinished, weak_ptr_factory_.GetWeakPtr(),
                     writer));

  source->GetDataForPreferredMimeTypes(
      base::BindOnce(&Seat::OnTextRead, weak_ptr_factory_.GetWeakPtr(), writer,
                     data_read_callback),
      base::BindOnce(&Seat::OnRTFRead, weak_ptr_factory_.GetWeakPtr(), writer,
                     data_read_callback),
      base::BindOnce(&Seat::OnHTMLRead, weak_ptr_factory_.GetWeakPtr(), writer,
                     data_read_callback),
      base::BindOnce(&Seat::OnImageRead, weak_ptr_factory_.GetWeakPtr(), writer,
                     data_read_callback),
      data_read_callback);
}

void Seat::OnTextRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                      base::OnceClosure callback,
                      const std::string& mime_type,
                      const std::vector<uint8_t>& data) {
  base::string16 output = CodepageToUTF16(data, GetCharset(mime_type));
  writer->WriteText(output);
  std::move(callback).Run();
}

void Seat::OnRTFRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                     base::OnceClosure callback,
                     const std::string& mime_type,
                     const std::vector<uint8_t>& data) {
  writer->WriteRTF(
      std::string(reinterpret_cast<const char*>(data.data()), data.size()));
  std::move(callback).Run();
}

void Seat::OnHTMLRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                      base::OnceClosure callback,
                      const std::string& mime_type,
                      const std::vector<uint8_t>& data) {
  base::string16 output = CodepageToUTF16(data, GetCharset(mime_type));
  writer->WriteHTML(output, std::string());
  std::move(callback).Run();
}

void Seat::OnImageRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                       base::OnceClosure callback,
                       const std::string& mime_type,
                       const std::vector<uint8_t>& data) {
#if defined(OS_CHROMEOS)
  data_decoder::DecodeImage(
      ash::Shell::Get()->connector(), data,
      data_decoder::mojom::ImageCodec::DEFAULT, false,
      std::numeric_limits<int64_t>::max(), gfx::Size(),
      base::BindOnce(&Seat::OnImageDecoded, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback), writer));
#else
  std::move(callback).Run();
#endif  // defined(OS_CHROMEOS)
}

#if defined(OS_CHROMEOS)
void Seat::OnImageDecoded(base::OnceClosure callback,
                          scoped_refptr<RefCountedScopedClipboardWriter> writer,
                          const SkBitmap& bitmap) {
  if (!bitmap.isNull() && !bitmap.empty())
    writer->WriteImage(bitmap);
  std::move(callback).Run();
}
#endif  // defined(OS_CHROMEOS)

void Seat::OnAllReadsFinished(
    scoped_refptr<RefCountedScopedClipboardWriter> writer) {
  // We need to destroy the ScopedClipboardWriter in this call, before
  // |auto_reset| is destroyed, so if there are outstanding references that
  // would prevent that, reschedule this task.
  if (!writer->HasOneRef()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&Seat::OnAllReadsFinished,
                       weak_ptr_factory_.GetWeakPtr(), std::move(writer)));
    return;
  }
  base::AutoReset<bool> auto_reset(
      &changing_clipboard_data_to_selection_source_, true);
  writer.reset();
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::FocusChangeObserver overrides:

void Seat::OnWindowFocused(aura::Window* gained_focus,
                           aura::Window* lost_focus) {
  Surface* const surface = GetEffectiveFocus(gained_focus);
  for (auto& observer : observers_) {
    observer.OnSurfaceFocusing(surface);
  }
  for (auto& observer : observers_) {
    observer.OnSurfaceFocused(surface);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::PlatformEventObserver overrides:

void Seat::WillProcessEvent(const ui::PlatformEvent& event) {
  switch (ui::EventTypeFromNative(event)) {
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED:
      physical_code_for_currently_processing_event_ = ui::CodeFromNative(event);
      break;
    default:
      break;
  }
}

void Seat::DidProcessEvent(const ui::PlatformEvent& event) {
  switch (ui::EventTypeFromNative(event)) {
    case ui::ET_KEY_PRESSED:
      physical_code_for_currently_processing_event_ = ui::DomCode::NONE;
      break;
    case ui::ET_KEY_RELEASED:
      // Remove this from the pressed key map because when IME is active we can
      // end up getting the DidProcessEvent call before we get the OnKeyEvent
      // callback and then the key will end up being stuck pressed.
      if (physical_code_for_currently_processing_event_ != ui::DomCode::NONE) {
        pressed_keys_.erase(physical_code_for_currently_processing_event_);
        physical_code_for_currently_processing_event_ = ui::DomCode::NONE;
      }
      break;
    default:
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Seat::OnKeyEvent(ui::KeyEvent* event) {
  // Ignore synthetic key repeat events.
  if (event->is_repeat())
    return;
  if (physical_code_for_currently_processing_event_ != ui::DomCode::NONE) {
    switch (event->type()) {
      case ui::ET_KEY_PRESSED:
        pressed_keys_.insert(
            {physical_code_for_currently_processing_event_, event->code()});
        break;
      case ui::ET_KEY_RELEASED:
        pressed_keys_.erase(physical_code_for_currently_processing_event_);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  modifier_flags_ = event->flags();
}

////////////////////////////////////////////////////////////////////////////////
// ui::ClipboardObserver overrides:

void Seat::OnClipboardDataChanged() {
  if (!selection_source_ || changing_clipboard_data_to_selection_source_)
    return;
  selection_source_->get()->Cancelled();
  selection_source_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// DataSourceObserver overrides:

void Seat::OnDataSourceDestroying(DataSource* source) {
  if (selection_source_ && selection_source_->get() == source)
    selection_source_.reset();
}

}  // namespace exo
