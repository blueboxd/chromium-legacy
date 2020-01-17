// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"

#include <memory>
#include <utility>

#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_item_options.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_reader.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

// There are 2 clipboard permissions defined in the spec:
// * clipboard-read
// * clipboard-write
// See https://w3c.github.io/clipboard-apis/#clipboard-permissions
//
// These permissions map to these ContentSettings:
// * CLIPBOARD_READ_WRITE, for sanitized read, and unsanitized read/write.
// * CLIPBOARD_SANITIZED_WRITE, for sanitized write only.

namespace blink {

using mojom::blink::PermissionStatus;
using mojom::blink::PermissionService;

// static
ScriptPromise ClipboardPromise::CreateForRead(SystemClipboard* system_clipboard,
                                              ScriptState* script_state) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(system_clipboard, script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleRead,
                           WrapPersistent(clipboard_promise)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

// static
ScriptPromise ClipboardPromise::CreateForReadText(
    SystemClipboard* system_clipboard,
    ScriptState* script_state) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(system_clipboard, script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleReadText,
                           WrapPersistent(clipboard_promise)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

// static
ScriptPromise ClipboardPromise::CreateForWrite(
    SystemClipboard* system_clipboard,
    ScriptState* script_state,
    const HeapVector<Member<ClipboardItem>>& items) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(system_clipboard, script_state);
  HeapVector<Member<ClipboardItem>>* items_copy =
      MakeGarbageCollected<HeapVector<Member<ClipboardItem>>>(items);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE,
      WTF::Bind(&ClipboardPromise::HandleWrite,
                WrapPersistent(clipboard_promise), WrapPersistent(items_copy)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

// static
ScriptPromise ClipboardPromise::CreateForWriteText(
    SystemClipboard* system_clipboard,
    ScriptState* script_state,
    const String& data) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(system_clipboard, script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleWriteText,
                           WrapPersistent(clipboard_promise), data));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ClipboardPromise::ClipboardPromise(SystemClipboard* system_clipboard,
                                   ScriptState* script_state)
    : ContextLifecycleObserver(blink::ExecutionContext::From(script_state)),
      script_state_(script_state),
      script_promise_resolver_(
          MakeGarbageCollected<ScriptPromiseResolver>(script_state)),
      clipboard_representation_index_(0),
      system_clipboard_(system_clipboard) {}

ClipboardPromise::~ClipboardPromise() = default;

void ClipboardPromise::CompleteWriteRepresentation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clipboard_writer_.Clear();  // The previous write is done.
  ++clipboard_representation_index_;
  StartWriteRepresentation();
}

void ClipboardPromise::StartWriteRepresentation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Commit to system clipboard when all representations are written.
  // This is in the start flow so that a |clipboard_item_data_| with 0 items
  // will still commit gracefully.
  if (clipboard_representation_index_ == clipboard_item_data_.size()) {
    system_clipboard_->CommitWrite();
    script_promise_resolver_->Resolve();
    return;
  }
  const String& type =
      clipboard_item_data_[clipboard_representation_index_].first;
  const Member<Blob>& blob =
      clipboard_item_data_[clipboard_representation_index_].second;

  DCHECK(!clipboard_writer_);
  clipboard_writer_ =
      ClipboardWriter::Create(system_clipboard_, type, is_raw_, this);
  clipboard_writer_->WriteToSystem(blob);
}

void ClipboardPromise::RejectFromReadOrDecodeFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kDataError,
      "Failed to read or decode Blob for clipboard item type " +
          clipboard_item_data_[clipboard_representation_index_].first + "."));
}

void ClipboardPromise::HandleRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestPermission(mojom::blink::PermissionName::CLIPBOARD_READ, false,
                    WTF::Bind(&ClipboardPromise::HandleReadWithPermission,
                              WrapPersistent(this)));
}

void ClipboardPromise::HandleReadText() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestPermission(mojom::blink::PermissionName::CLIPBOARD_READ, false,
                    WTF::Bind(&ClipboardPromise::HandleReadTextWithPermission,
                              WrapPersistent(this)));
}

void ClipboardPromise::HandleWrite(
    HeapVector<Member<ClipboardItem>>* clipboard_items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(clipboard_items);

  if (clipboard_items->size() > 1) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Support for multiple ClipboardItems is not implemented."));
    return;
  }
  if (!clipboard_items->size()) {
    // Do nothing if there are no ClipboardItems.
    script_promise_resolver_->Resolve();
    return;
  }

  // For now, we only process the first ClipboardItem.
  ClipboardItem* clipboard_item = (*clipboard_items)[0];
  clipboard_item_data_ = clipboard_item->GetItems();
  is_raw_ = clipboard_item->raw();

  DCHECK(base::FeatureList::IsEnabled(blink::features::kRawClipboard) ||
         !is_raw_);

  RequestPermission(mojom::blink::PermissionName::CLIPBOARD_WRITE, is_raw_,
                    WTF::Bind(&ClipboardPromise::HandleWriteWithPermission,
                              WrapPersistent(this)));
}

void ClipboardPromise::HandleWriteText(const String& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  plain_text_ = data;
  RequestPermission(mojom::blink::PermissionName::CLIPBOARD_WRITE, false,
                    WTF::Bind(&ClipboardPromise::HandleWriteTextWithPermission,
                              WrapPersistent(this)));
}

void ClipboardPromise::HandleReadWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Read permission denied."));
    return;
  }

  Vector<String> available_types = system_clipboard_->ReadAvailableTypes();
  HeapVector<std::pair<String, Member<Blob>>> items;
  items.ReserveInitialCapacity(available_types.size());
  for (String& type_to_read : available_types) {
    ClipboardReader* reader =
        ClipboardReader::Create(system_clipboard_, type_to_read);
    if (reader)
      items.emplace_back(std::move(type_to_read), reader->ReadFromSystem());
  }

  if (!items.size()) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError, "No valid data on clipboard."));
    return;
  }

  ClipboardItemOptions* options = ClipboardItemOptions::Create();
  options->setRaw(false);

  HeapVector<Member<ClipboardItem>> clipboard_items = {
      MakeGarbageCollected<ClipboardItem>(items, options)};
  script_promise_resolver_->Resolve(clipboard_items);
}

void ClipboardPromise::HandleReadTextWithPermission(PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Read permission denied."));
    return;
  }

  String text =
      system_clipboard_->ReadPlainText(mojom::ClipboardBuffer::kStandard);
  script_promise_resolver_->Resolve(text);
}

void ClipboardPromise::HandleWriteWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Write permission denied."));
    return;
  }

  // Check that all blobs have valid MIME types.
  // Also, Blobs may have a full MIME type with args
  // (ex. 'text/plain;charset=utf-8'), whereas the type must not have args
  // (ex. 'text/plain' only), so ensure that Blob->type is contained in type.
  for (const auto& type_and_blob : clipboard_item_data_) {
    String type = type_and_blob.first;
    String type_with_args = type_and_blob.second->type();
    if (!is_raw_ && !ClipboardWriter::IsValidType(type)) {
      script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Sanitized MIME type " + type + " not supported on write."));
      return;
    }
    if (!type_with_args.Contains(type)) {
      script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "MIME type " + type + " does not match the blob type's MIME type " +
              type_with_args));
      return;
    }
  }

  DCHECK(!clipboard_representation_index_);
  StartWriteRepresentation();
}

void ClipboardPromise::HandleWriteTextWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Write permission denied."));
    return;
  }

  system_clipboard_->WritePlainText(plain_text_);
  system_clipboard_->CommitWrite();
  script_promise_resolver_->Resolve();
}

PermissionService* ClipboardPromise::GetPermissionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!permission_service_) {
    ConnectToPermissionService(
        ExecutionContext::From(script_state_),
        permission_service_.BindNewPipeAndPassReceiver());
  }
  return permission_service_.get();
}

void ClipboardPromise::RequestPermission(
    mojom::blink::PermissionName permission,
    bool allow_without_sanitization,
    base::OnceCallback<void(::blink::mojom::PermissionStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(script_promise_resolver_);
  DCHECK(permission == mojom::blink::PermissionName::CLIPBOARD_READ ||
         permission == mojom::blink::PermissionName::CLIPBOARD_WRITE);

  ExecutionContext* context = ExecutionContext::From(script_state_);
  DCHECK(context);
  const Document& document = *To<Document>(context);
  DCHECK(document.IsSecureContext());  // [SecureContext] in IDL

  if (!document.hasFocus()) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Document is not focused."));
    return;
  }

  if (!document.IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kClipboard,
          ReportOptions::kReportOnFailure,
          "The Clipboard API has been blocked because of a Feature Policy "
          "applied to the current document. See https://goo.gl/EuHzyv for more "
          "details.")) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Disabled in this document by Feature Policy."));
    return;
  }

  if (!GetPermissionService()) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Permission Service could not connect."));
    return;
  }

  auto permission_descriptor = CreateClipboardPermissionDescriptor(
      permission, false, allow_without_sanitization);
  if (permission == mojom::blink::PermissionName::CLIPBOARD_WRITE &&
      !allow_without_sanitization) {
    // Check permission (but do not query the user).
    // See crbug.com/795929 for moving this check into the Browser process.
    permission_service_->HasPermission(std::move(permission_descriptor),
                                       std::move(callback));
    return;
  }
  // Check permission, and query if necessary.
  // See crbug.com/795929 for moving this check into the Browser process.
  permission_service_->RequestPermission(std::move(permission_descriptor),
                                         false, std::move(callback));
}

scoped_refptr<base::SingleThreadTaskRunner> ClipboardPromise::GetTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Get the User Interaction task runner, as Async Clipboard API calls require
  // user interaction, as specified in https://w3c.github.io/clipboard-apis/
  return GetExecutionContext()->GetTaskRunner(TaskType::kUserInteraction);
}

void ClipboardPromise::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(script_promise_resolver_);
  visitor->Trace(clipboard_writer_);
  visitor->Trace(clipboard_item_data_);
  visitor->Trace(system_clipboard_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
