// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"

#include <utility>

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/modules/nfc/nfc_reader.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"

namespace blink {

// static
const char NFCProxy::kSupplementName[] = "NFCProxy";

// static
NFCProxy* NFCProxy::From(Document& document) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!document.IsInMainFrame()) {
    return nullptr;
  }

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
      Supplement<Document>(document),
      client_binding_(this) {}

NFCProxy::~NFCProxy() = default;

void NFCProxy::Dispose() {
  client_binding_.Close();
}

void NFCProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(readers_);
  PageVisibilityObserver::Trace(visitor);
  Supplement<Document>::Trace(visitor);
}

void NFCProxy::AddReader(NFCReader* reader) {
  DCHECK(reader);

  if (readers_.Contains(reader))
    return;

  EnsureMojoConnection();
  nfc_->Watch(reader->options().Clone(),
              WTF::Bind(&NFCProxy::OnReaderRegistered, WrapPersistent(this),
                        WrapPersistent(reader)));
}

void NFCProxy::RemoveReader(NFCReader* reader) {
  auto iter = readers_.find(reader);
  if (iter != readers_.end()) {
    nfc_->CancelWatch(iter->value,
                      WTF::Bind(&NFCProxy::OnCancelWatch, WrapPersistent(this),
                                WrapPersistent(reader)));
    readers_.erase(iter);
  }
}

void NFCProxy::Push(device::mojom::blink::NDEFMessagePtr message,
                    device::mojom::blink::NFCPushOptionsPtr options,
                    device::mojom::blink::NFC::PushCallback cb) {
  EnsureMojoConnection();
  nfc_->Push(std::move(message), std::move(options), std::move(cb));
}

void NFCProxy::PageVisibilityChanged() {
  // If service is not initialized, there cannot be any pending NFC activities
  if (!nfc_)
    return;

  // NFC operations should be suspended.
  // https://w3c.github.io/web-nfc/#nfc-suspended
  if (GetPage()->IsPageVisible())
    nfc_->ResumeNFCOperations();
  else
    nfc_->SuspendNFCOperations();
}

// device::mojom::blink::NFCClient implementation.
void NFCProxy::OnWatch(const Vector<uint32_t>& ids,
                       device::mojom::blink::NDEFMessagePtr message) {
  // Dispatch the event to all matched and registered observers.
  // All readers in the list should be active.
  for (auto pair : readers_) {
    if (ids.Contains(pair.value)) {
      pair.key->OnMessage(*message);
    }
  }
}

void NFCProxy::OnReaderRegistered(NFCReader* reader,
                                  uint32_t id,
                                  device::mojom::blink::NFCErrorPtr error) {
  DCHECK(reader);

  if (error) {
    reader->OnError(*error);
    return;
  }

  readers_.insert(reader, id);
}

void NFCProxy::OnCancelWatch(NFCReader* reader,
                             device::mojom::blink::NFCErrorPtr error) {
  DCHECK(reader);

  if (error) {
    reader->OnError(*error);
  }
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
  device::mojom::blink::NFCClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client, task_runner), task_runner);
  nfc_->SetClient(std::move(client));
}

void NFCProxy::OnMojoConnectionError() {
  nfc_.reset();

  // Notify all active readers about the connection error.
  // NFCWriter::push() should wrap the callback using
  // mojo::WrapCallbackWithDefaultInvokeIfNotRun() with a default error.
  auto error = device::mojom::blink::NFCError::New(
      device::mojom::blink::NFCErrorType::NOT_SUPPORTED);
  for (auto pair : readers_) {
    pair.key->OnError(*error);
  }

  // Clear the reader list.
  readers_.clear();
}

}  // namespace blink
