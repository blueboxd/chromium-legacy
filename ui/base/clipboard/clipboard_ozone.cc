// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_ozone.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

namespace {

// The amount of time to wait for a request to complete before aborting it.
constexpr base::TimeDelta kRequestTimeoutMs =
    base::TimeDelta::FromMilliseconds(10000);

}  // namespace

// A helper class, which uses a request pattern to asynchronously communicate
// with the ozone::PlatformClipboard and fetch clipboard data with mimes
// specified.
class ClipboardOzone::AsyncClipboardOzone {
 public:
  explicit AsyncClipboardOzone(PlatformClipboard* platform_clipboard)
      : platform_clipboard_(platform_clipboard), weak_factory_(this) {
    // Set a callback to listen to requests to increase the clipboard sequence
    // number.
    auto update_sequence_cb =
        base::BindRepeating(&AsyncClipboardOzone::UpdateClipboardSequenceNumber,
                            weak_factory_.GetWeakPtr());
    platform_clipboard_->SetSequenceNumberUpdateCb(
        std::move(update_sequence_cb));
  }

  ~AsyncClipboardOzone() = default;

  base::span<uint8_t> ReadClipboardDataAndWait(ClipboardType type,
                                               const std::string& mime_type) {
    // TODO(tonikitoo): add selection support.
    if (type == ClipboardType::kSelection)
      return base::span<uint8_t>();

    // We can use a fastpath if we are the owner of the selection.
    if (platform_clipboard_->IsSelectionOwner()) {
      auto it = offered_data_.find(mime_type);
      if (it == offered_data_.end())
        return {};
      return base::make_span(it->second.data(), it->second.size());
    }

    Request request(RequestType::kRead);
    request.requested_mime_type = mime_type;
    PerformRequestAndWaitForResult(&request);

    offered_data_ = std::move(request.data_map);
    auto it = offered_data_.find(mime_type);
    if (it == offered_data_.end())
      return {};
    return base::make_span(it->second.data(), it->second.size());
  }

  std::vector<std::string> RequestMimeTypes() {
    // We can use a fastpath if we are the owner of the selection.
    if (platform_clipboard_->IsSelectionOwner()) {
      std::vector<std::string> mime_types;
      for (const auto& item : offered_data_)
        mime_types.push_back(item.first);
      return mime_types;
    }

    Request request(RequestType::kGetMime);
    PerformRequestAndWaitForResult(&request);
    return request.mime_types;
  }

  void OfferData() {
    Request request(RequestType::kOffer);
    request.data_map = offered_data_;
    PerformRequestAndWaitForResult(&request);

    UpdateClipboardSequenceNumber();
  }

  void InsertData(std::vector<uint8_t> data, const std::string& mime_type) {
    DCHECK(offered_data_.find(mime_type) == offered_data_.end());
    offered_data_[mime_type] = std::move(data);
  }

  void ClearOfferedData() { offered_data_.clear(); }

  uint64_t GetSequenceNumber(ClipboardType type) {
    if (type == ClipboardType::kCopyPaste)
      return clipboard_sequence_number_;
    // TODO(tonikitoo): add sequence number for the selection clipboard type.
    return 0;
  }

 private:
  enum class RequestType {
    kRead = 0,
    kOffer = 1,
    kGetMime = 2,
  };

  // A structure, which holds request data to process inquiries from
  // the ClipboardOzone.
  struct Request {
    explicit Request(RequestType request_type) : type(request_type) {}
    ~Request() = default;

    // Describes the type of the request.
    RequestType type;

    // A closure that is used to signal the request is processed.
    base::OnceClosure finish_closure;

    // Used for kRead and kOffer requests. It contains either data offered by
    // Chromium to a system clipboard or a read data offered by the system
    // clipboard.
    PlatformClipboard::DataMap data_map;

    // Identifies which mime type the client is interested to read from the
    // system clipboard during kRead requests.
    std::string requested_mime_type;

    // A vector of mime types returned as a result to a kGetMime request to get
    // available mime types.
    std::vector<std::string> mime_types;
  };

  void PerformRequestAndWaitForResult(Request* request) {
    DCHECK(request);
    DCHECK(!abort_timer_.IsRunning());
    DCHECK(!pending_request_);

    pending_request_ = request;
    switch (pending_request_->type) {
      case (RequestType::kRead):
        DispatchReadRequest(request);
        break;
      case (RequestType::kOffer):
        DispatchOfferRequest(request);
        break;
      case (RequestType::kGetMime):
        DispatchGetMimeRequest(request);
        break;
    }

    if (!pending_request_)
      return;

    // TODO(https://crbug.com/913422): the implementation is known to be
    // dangerous, and may cause blocks in ui thread. But base::Clipboard was
    // designed to have synchrous APIs rather than asynchronous ones that at
    // least two system clipboards on X11 and Wayland provide.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    request->finish_closure = run_loop.QuitClosure();

    // Set a timeout timer after which the request will be aborted.
    abort_timer_.Start(FROM_HERE, kRequestTimeoutMs, this,
                       &AsyncClipboardOzone::AbortStalledRequest);
    run_loop.Run();
  }

  void AbortStalledRequest() {
    if (pending_request_ && pending_request_->finish_closure)
      std::move(pending_request_->finish_closure).Run();
  }

  void DispatchReadRequest(Request* request) {
    auto callback = base::BindOnce(&AsyncClipboardOzone::OnTextRead,
                                   weak_factory_.GetWeakPtr());
    DCHECK(platform_clipboard_);
    platform_clipboard_->RequestClipboardData(
        request->requested_mime_type, &request->data_map, std::move(callback));
  }

  void DispatchOfferRequest(Request* request) {
    auto callback = base::BindOnce(&AsyncClipboardOzone::OnOfferDone,
                                   weak_factory_.GetWeakPtr());
    DCHECK(platform_clipboard_);
    platform_clipboard_->OfferClipboardData(request->data_map,
                                            std::move(callback));
  }

  void DispatchGetMimeRequest(Request* request) {
    auto callback = base::BindOnce(&AsyncClipboardOzone::OnGotMimeTypes,
                                   weak_factory_.GetWeakPtr());
    DCHECK(platform_clipboard_);
    platform_clipboard_->GetAvailableMimeTypes(std::move(callback));
  }

  void OnTextRead(const base::Optional<std::vector<uint8_t>>& data) {
    // |data| is already set in request's data_map, so just finish request
    // processing.
    CompleteRequest();
  }

  void OnOfferDone() { CompleteRequest(); }

  void OnGotMimeTypes(const std::vector<std::string>& mime_types) {
    pending_request_->mime_types = std::move(mime_types);
    CompleteRequest();
  }

  void CompleteRequest() {
    if (!pending_request_)
      return;
    abort_timer_.Stop();
    if (pending_request_->finish_closure)
      std::move(pending_request_->finish_closure).Run();
    pending_request_ = nullptr;
  }

  void UpdateClipboardSequenceNumber() { ++clipboard_sequence_number_; }

  // Cached clipboard data, which is pending to be written. Must be cleared on
  // every new write to the |platform_clipboard_|.
  PlatformClipboard::DataMap offered_data_;

  // A current pending request being processed.
  Request* pending_request_ = nullptr;

  // Aborts |pending_request| after Request::timeout.
  base::RepeatingTimer abort_timer_;

  // Provides communication to a system clipboard under ozone level.
  PlatformClipboard* platform_clipboard_ = nullptr;

  uint64_t clipboard_sequence_number_ = 0;

  base::WeakPtrFactory<AsyncClipboardOzone> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AsyncClipboardOzone);
};

// Clipboard factory method.
Clipboard* Clipboard::Create() {
  return new ClipboardOzone;
}

// ClipboardOzone implementation.
ClipboardOzone::ClipboardOzone() {
  async_clipboard_ozone_ =
      std::make_unique<ClipboardOzone::AsyncClipboardOzone>(
          OzonePlatform::GetInstance()->GetPlatformClipboard());
}

ClipboardOzone::~ClipboardOzone() = default;

void ClipboardOzone::OnPreShutdown() {}

uint64_t ClipboardOzone::GetSequenceNumber(ClipboardType type) const {
  return async_clipboard_ozone_->GetSequenceNumber(type);
}

bool ClipboardOzone::IsFormatAvailable(const ClipboardFormatType& format,
                                       ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  // TODO(tonikitoo): add selection support.
  if (type == ClipboardType::kSelection)
    return false;

  auto available_types = async_clipboard_ozone_->RequestMimeTypes();
  for (auto mime_type : available_types) {
    if (format.ToString() == mime_type) {
      return true;
    }
  }
  return false;
}

void ClipboardOzone::Clear(ClipboardType type) {
  async_clipboard_ozone_->ClearOfferedData();
  async_clipboard_ozone_->OfferData();
}

void ClipboardOzone::ReadAvailableTypes(ClipboardType type,
                                        std::vector<base::string16>* types,
                                        bool* contains_filenames) const {
  DCHECK(CalledOnValidThread());
  types->clear();

  // TODO(tonikitoo): add selection support.
  if (type == ClipboardType::kSelection)
    return;

  auto available_types = async_clipboard_ozone_->RequestMimeTypes();
  for (auto mime_type : available_types)
    types->push_back(base::UTF8ToUTF16(mime_type));
}

void ClipboardOzone::ReadText(ClipboardType type,
                              base::string16* result) const {
  DCHECK(CalledOnValidThread());

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(type, kMimeTypeText);
  *result = base::UTF8ToUTF16(base::StringPiece(
      reinterpret_cast<char*>(clipboard_data.data()), clipboard_data.size()));
}

void ClipboardOzone::ReadAsciiText(ClipboardType type,
                                   std::string* result) const {
  DCHECK(CalledOnValidThread());
  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(type, kMimeTypeText);
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

void ClipboardOzone::ReadHTML(ClipboardType type,
                              base::string16* markup,
                              std::string* src_url,
                              uint32_t* fragment_start,
                              uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  markup->clear();
  if (src_url)
    src_url->clear();
  *fragment_start = 0;
  *fragment_end = 0;

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(type, kMimeTypeHTML);
  *markup = base::UTF8ToUTF16(base::StringPiece(
      reinterpret_cast<char*>(clipboard_data.data()), clipboard_data.size()));
  DCHECK(markup->length() <= std::numeric_limits<uint32_t>::max());
  *fragment_end = static_cast<uint32_t>(markup->length());
}

void ClipboardOzone::ReadRTF(ClipboardType type, std::string* result) const {
  DCHECK(CalledOnValidThread());
  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(type, kMimeTypeRTF);
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

SkBitmap ClipboardOzone::ReadImage(ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(type, kMimeTypePNG);
  SkBitmap bitmap;
  if (gfx::PNGCodec::Decode(clipboard_data.data(), clipboard_data.size(),
                            &bitmap))
    return SkBitmap(bitmap);
  return SkBitmap();
}

void ClipboardOzone::ReadCustomData(ClipboardType clipboard_type,
                                    const base::string16& type,
                                    base::string16* result) const {
  DCHECK(CalledOnValidThread());
  auto custom_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      clipboard_type, kMimeTypeWebCustomData);
  ui::ReadCustomDataForType(custom_data.data(), custom_data.size(), type,
                            result);
}

void ClipboardOzone::ReadBookmark(base::string16* title,
                                  std::string* url) const {
  DCHECK(CalledOnValidThread());
  // TODO(msisov): This was left NOTIMPLEMENTED() in all the Linux platforms.
  NOTIMPLEMENTED();
}

void ClipboardOzone::ReadData(const ClipboardFormatType& format,
                              std::string* result) const {
  DCHECK(CalledOnValidThread());
  auto clipboard_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      ClipboardType::kCopyPaste, format.ToString());
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

void ClipboardOzone::WriteObjects(ClipboardType type,
                                  const ObjectMap& objects) {
  DCHECK(CalledOnValidThread());
  if (type == ClipboardType::kCopyPaste) {
    async_clipboard_ozone_->ClearOfferedData();

    for (const auto& object : objects)
      DispatchObject(static_cast<ObjectType>(object.first), object.second);

    async_clipboard_ozone_->OfferData();
  }
}

void ClipboardOzone::WriteText(const char* text_data, size_t text_len) {
  std::vector<uint8_t> data(text_data, text_data + text_len);
  async_clipboard_ozone_->InsertData(std::move(data), kMimeTypeText);
}

void ClipboardOzone::WriteHTML(const char* markup_data,
                               size_t markup_len,
                               const char* url_data,
                               size_t url_len) {
  std::vector<uint8_t> data(markup_data, markup_data + markup_len);
  async_clipboard_ozone_->InsertData(std::move(data), kMimeTypeHTML);
}

void ClipboardOzone::WriteRTF(const char* rtf_data, size_t data_len) {
  std::vector<uint8_t> data(rtf_data, rtf_data + data_len);
  async_clipboard_ozone_->InsertData(std::move(data), kMimeTypeRTF);
}

void ClipboardOzone::WriteBookmark(const char* title_data,
                                   size_t title_len,
                                   const char* url_data,
                                   size_t url_len) {
  // Writes a Mozilla url (UTF16: URL, newline, title)
  base::string16 bookmark =
      base::UTF8ToUTF16(base::StringPiece(url_data, url_len)) +
      base::ASCIIToUTF16("\n") +
      base::UTF8ToUTF16(base::StringPiece(title_data, title_len));

  std::vector<uint8_t> data(
      reinterpret_cast<const uint8_t*>(bookmark.data()),
      reinterpret_cast<const uint8_t*>(bookmark.data() + bookmark.size()));
  async_clipboard_ozone_->InsertData(std::move(data), kMimeTypeMozillaURL);
}

void ClipboardOzone::WriteWebSmartPaste() {
  async_clipboard_ozone_->InsertData(std::vector<uint8_t>(),
                                     kMimeTypeWebkitSmartPaste);
}

void ClipboardOzone::WriteBitmap(const SkBitmap& bitmap) {
  std::vector<unsigned char> output;
  if (gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, false, &output))
    async_clipboard_ozone_->InsertData(std::move(output), kMimeTypePNG);
}

void ClipboardOzone::WriteData(const ClipboardFormatType& format,
                               const char* data_data,
                               size_t data_len) {
  std::vector<uint8_t> data(data_data, data_data + data_len);
  async_clipboard_ozone_->InsertData(data, format.ToString());
}

}  // namespace ui
