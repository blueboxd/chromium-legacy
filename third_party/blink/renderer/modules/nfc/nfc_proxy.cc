// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"

#include <utility>

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/modules/nfc/nfc_reader.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/modules/nfc/nfc_writer.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"

namespace blink {

namespace {
constexpr uint32_t kInvalidNfcWatchId = 0;
}

// static
const char NFCProxy::kSupplementName[] = "NFCProxy";

// static
NFCProxy* NFCProxy::From(Document& document) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  DCHECK(document.IsInMainFrame());

  NFCProxy* nfc_proxy = Supplement<Document>::From<NFCProxy>(document);
  if (!nfc_proxy) {
    nfc_proxy = MakeGarbageCollected<NFCProxy>(document);
    Supplement<Document>::ProvideTo(document, nfc_proxy);
  }
  return nfc_proxy;
}

// NFCProxy
NFCProxy::NFCProxy(Document& document)
    : PageVisibilityObserver(document.GetPage()),
      FocusChangedObserver(document.GetPage()),
      Supplement<Document>(document),
      client_binding_(this) {}

NFCProxy::~NFCProxy() = default;

void NFCProxy::Dispose() {
  client_binding_.Close();
}

void NFCProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(writers_);
  visitor->Trace(readers_);
  PageVisibilityObserver::Trace(visitor);
  FocusChangedObserver::Trace(visitor);
  Supplement<Document>::Trace(visitor);
}

void NFCProxy::StartReading(NFCReader* reader) {
  DCHECK(reader);
  if (readers_.Contains(reader))
    return;

  EnsureMojoConnection();
  nfc_->Watch(device::mojom::blink::NFCReaderOptions::From(reader->options()),
              WTF::Bind(&NFCProxy::OnReaderRegistered, WrapPersistent(this),
                        WrapPersistent(reader)));
  readers_.insert(reader, kInvalidNfcWatchId);
}

void NFCProxy::StopReading(NFCReader* reader) {
  DCHECK(reader);
  auto iter = readers_.find(reader);
  if (iter != readers_.end()) {
    if (nfc_ && iter->value != kInvalidNfcWatchId) {
      // We do not need to notify |reader| of anything.
      nfc_->CancelWatch(iter->value,
                        device::mojom::blink::NFC::CancelWatchCallback());
    }
    readers_.erase(iter);
  }
}

bool NFCProxy::IsReading(const NFCReader* reader) {
  DCHECK(reader);
  return readers_.Contains(const_cast<NFCReader*>(reader));
}

void NFCProxy::AddWriter(NFCWriter* writer) {
  DCHECK(!writers_.Contains(writer));
  writers_.insert(writer);
}

void NFCProxy::Push(device::mojom::blink::NDEFMessagePtr message,
                    device::mojom::blink::NFCPushOptionsPtr options,
                    device::mojom::blink::NFC::PushCallback cb) {
  EnsureMojoConnection();
  nfc_->Push(std::move(message), std::move(options), std::move(cb));
}

void NFCProxy::CancelPush(
    const String& target,
    device::mojom::blink::NFC::CancelPushCallback callback) {
  DCHECK(nfc_);
  nfc_->CancelPush(StringToNFCPushTarget(target), std::move(callback));
}

// device::mojom::blink::NFCClient implementation.
void NFCProxy::OnWatch(const Vector<uint32_t>& ids,
                       const String& serial_number,
                       device::mojom::blink::NDEFMessagePtr message) {
  // Dispatch the event to all matched readers. Of course readers with
  // kInvalidNfcWatchId do not match.
  // This loop is O(n^2), however, we assume the number of readers to be small
  // so it'd be just OK.
  for (auto pair : readers_) {
    if (ids.Contains(pair.value)) {
      pair.key->OnReading(serial_number, *message);
    }
  }
}

void NFCProxy::OnReaderRegistered(NFCReader* reader,
                                  uint32_t id,
                                  device::mojom::blink::NFCErrorPtr error) {
  DCHECK(reader);
  // |reader| may have already stopped reading.
  if (!readers_.Contains(reader))
    return;

  if (error) {
    reader->OnError(error->error_type);
    readers_.erase(reader);
    return;
  }

  DCHECK_NE(id, kInvalidNfcWatchId);
  readers_.Set(reader, id);
}

void NFCProxy::PageVisibilityChanged() {
  UpdateSuspendedStatus();
}

void NFCProxy::FocusedFrameChanged() {
  UpdateSuspendedStatus();
}

void NFCProxy::UpdateSuspendedStatus() {
  // If service is not initialized, there cannot be any pending NFC activities.
  if (!nfc_)
    return;

  // NFC operations should be suspended.
  // https://w3c.github.io/web-nfc/#nfc-suspended
  // TODO(https://crbug.com/520391): Suspend/Resume NFC in the browser process
  // instead to prevent a compromised renderer from using NFC in the background.
  if (ShouldSuspendNFC())
    nfc_->SuspendNFCOperations();
  else
    nfc_->ResumeNFCOperations();
}

bool NFCProxy::ShouldSuspendNFC() const {
  if (!GetPage()->IsPageVisible())
    return true;

  LocalFrame* focused_frame = GetPage()->GetFocusController().FocusedFrame();
  LocalFrame* this_frame = GetSupplementable()->GetFrame();

  if (!focused_frame || !this_frame)
    return true;

  if (focused_frame != this_frame)
    return true;

  return false;
}

void NFCProxy::EnsureMojoConnection() {
  if (nfc_)
    return;

  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner =
      GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetSupplementable()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&nfc_, task_runner));
  nfc_.set_connection_error_handler(
      WTF::Bind(&NFCProxy::OnMojoConnectionError, WrapWeakPersistent(this)));

  // Set client for OnWatch event.
  DCHECK(!client_binding_);
  device::mojom::blink::NFCClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client, task_runner), task_runner);
  nfc_->SetClient(std::move(client));
}

void NFCProxy::OnMojoConnectionError() {
  nfc_.reset();
  client_binding_.Close();

  // Notify all active readers about the connection error and clear the list.
  ReaderMap readers = std::move(readers_);
  for (auto pair : readers) {
    // The reader may call StopReading() to remove itself from |readers_| when
    // handling the error.
    pair.key->OnError(device::mojom::blink::NFCErrorType::NOT_READABLE);
  }

  // Notify all writers about the connection error.
  for (auto& writer : writers_) {
    writer->OnMojoConnectionError();
  }
  // Clear the reader list.
  writers_.clear();
}

}  // namespace blink
