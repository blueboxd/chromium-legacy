// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_file_system/data_transfer_item_native_file_system.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_file_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom-blink.h"
#include "third_party/blink/renderer/core/clipboard/data_object_item.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_item.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_file_handle.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// static
NativeFileSystemHandle* DataTransferItemNativeFileSystem::getAsFileSystemHandle(
    ScriptState* script_state,
    DataTransferItem& data_transfer_item) {
  if (!data_transfer_item.GetDataTransfer()->CanReadData())
    return nullptr;

  // If the DataObjectItem doesn't have an associated NativeFileSystemEntry,
  // return nullptr.
  if (!data_transfer_item.GetDataObjectItem()->HasNativeFileSystemEntry()) {
    return nullptr;
  }

  mojo::Remote<mojom::blink::NativeFileSystemManager> nfs_manager;
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      nfs_manager.BindNewPipeAndPassReceiver());

  const DataObjectItem& data_object_item =
      *data_transfer_item.GetDataObjectItem();

  // Since tokens are move-only, we need to create a clone in order
  // to preserve the state of |data_object_item| for future calls.
  mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken>
      token_remote = data_object_item.CloneNativeFileSystemEntryToken();

  // Resolve and return the token as a
  // NativeFileSystemDirectoryHandle/NativeFileSystemFileHandle.
  if (data_object_item.NativeFileSystemEntryIsDirectory()) {
    mojo::PendingRemote<mojom::blink::NativeFileSystemDirectoryHandle>
        dir_remote;
    nfs_manager->GetDirectoryHandleFromToken(
        std::move(token_remote), dir_remote.InitWithNewPipeAndPassReceiver());
    return MakeGarbageCollected<blink::NativeFileSystemDirectoryHandle>(
        execution_context, data_object_item.NativeFileSystemFileName(),
        std::move(dir_remote));
  } else {
    mojo::PendingRemote<blink::mojom::blink::NativeFileSystemFileHandle>
        file_remote;
    nfs_manager->GetFileHandleFromToken(
        std::move(token_remote), file_remote.InitWithNewPipeAndPassReceiver());
    return MakeGarbageCollected<blink::NativeFileSystemFileHandle>(
        execution_context, data_object_item.NativeFileSystemFileName(),
        std::move(file_remote));
  }
}

}  // namespace blink
