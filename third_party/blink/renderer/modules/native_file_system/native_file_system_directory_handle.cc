// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_handle.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom-blink.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/native_file_system/file_system_get_directory_options.h"
#include "third_party/blink/renderer/modules/native_file_system/file_system_get_file_options.h"
#include "third_party/blink/renderer/modules/native_file_system/file_system_remove_options.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_iterator.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_file_handle.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {
// The name to use for the root directory of a sandboxed file system.
constexpr const char kSandboxRootDirectoryName[] = "";
}  // namespace

using mojom::blink::NativeFileSystemErrorPtr;

NativeFileSystemDirectoryHandle::NativeFileSystemDirectoryHandle(
    const String& name,
    RevocableInterfacePtr<mojom::blink::NativeFileSystemDirectoryHandle>
        mojo_ptr)
    : NativeFileSystemHandle(name), mojo_ptr_(std::move(mojo_ptr)) {
  DCHECK(mojo_ptr_);
}

ScriptPromise NativeFileSystemDirectoryHandle::getFile(
    ScriptState* script_state,
    const String& name,
    const FileSystemGetFileOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->GetFile(
      name, options->create(),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, const String& name,
             NativeFileSystemErrorPtr result,
             mojom::blink::NativeFileSystemFileHandlePtr handle) {
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context)
              return;
            if (result->error_code == base::File::FILE_OK) {
              resolver->Resolve(
                  MakeGarbageCollected<NativeFileSystemFileHandle>(
                      name,
                      RevocableInterfacePtr<
                          mojom::blink::NativeFileSystemFileHandle>(
                          handle.PassInterface(),
                          context->GetInterfaceInvalidator(),
                          context->GetTaskRunner(TaskType::kMiscPlatformAPI))));
            } else {
              resolver->Reject(
                  file_error::CreateDOMException(result->error_code));
            }
          },
          WrapPersistent(resolver), name));

  return result;
}

ScriptPromise NativeFileSystemDirectoryHandle::getDirectory(
    ScriptState* script_state,
    const String& name,
    const FileSystemGetDirectoryOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->GetDirectory(
      name, options->create(),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, const String& name,
             NativeFileSystemErrorPtr result,
             mojom::blink::NativeFileSystemDirectoryHandlePtr handle) {
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context)
              return;
            if (result->error_code == base::File::FILE_OK) {
              resolver->Resolve(
                  MakeGarbageCollected<NativeFileSystemDirectoryHandle>(
                      name,
                      RevocableInterfacePtr<
                          mojom::blink::NativeFileSystemDirectoryHandle>(
                          handle.PassInterface(),
                          context->GetInterfaceInvalidator(),
                          context->GetTaskRunner(TaskType::kMiscPlatformAPI))));
            } else {
              resolver->Reject(
                  file_error::CreateDOMException(result->error_code));
            }
          },
          WrapPersistent(resolver), name));

  return result;
}

namespace {

void ReturnDataFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, info.Data());
}

}  // namespace

ScriptValue NativeFileSystemDirectoryHandle::getEntries(
    ScriptState* script_state) {
  auto* iterator = MakeGarbageCollected<NativeFileSystemDirectoryIterator>(
      this, ExecutionContext::From(script_state));
  auto* isolate = script_state->GetIsolate();
  auto context = script_state->GetContext();
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  if (!result
           ->Set(context, v8::Symbol::GetAsyncIterator(isolate),
                 v8::Function::New(context, &ReturnDataFunction,
                                   ToV8(iterator, script_state))
                     .ToLocalChecked())
           .ToChecked()) {
    return ScriptValue();
  }
  return ScriptValue(script_state, result);
}

ScriptPromise NativeFileSystemDirectoryHandle::removeEntry(
    ScriptState* script_state,
    const String& name,
    const FileSystemRemoveOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->RemoveEntry(
      name, options->recursive(),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, NativeFileSystemErrorPtr result) {
            if (result->error_code == base::File::FILE_OK) {
              resolver->Resolve();
            } else {
              resolver->Reject(
                  file_error::CreateDOMException(result->error_code));
            }
          },
          WrapPersistent(resolver)));

  return result;
}

// static
ScriptPromise NativeFileSystemDirectoryHandle::getSystemDirectory(
    ScriptState* script_state,
    const GetSystemDirectoryOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  // TODO(mek): Cache NativeFileSystemManagerPtr associated with an
  // ExecutionContext, so we don't have to request a new one for each operation,
  // and can avoid code duplication between here and other uses.
  mojom::blink::NativeFileSystemManagerPtr manager;
  auto* provider = ExecutionContext::From(script_state)->GetInterfaceProvider();
  if (!provider) {
    resolver->Reject(file_error::CreateDOMException(
        base::File::FILE_ERROR_INVALID_OPERATION));
    return result;
  }

  provider->GetInterface(&manager);
  auto* raw_manager = manager.get();
  raw_manager->GetSandboxedFileSystem(WTF::Bind(
      [](ScriptPromiseResolver* resolver,
         mojom::blink::NativeFileSystemManagerPtr,
         NativeFileSystemErrorPtr result,
         mojom::blink::NativeFileSystemDirectoryHandlePtr handle) {
        ExecutionContext* context = resolver->GetExecutionContext();
        if (!context)
          return;
        if (result->error_code == base::File::FILE_OK) {
          resolver->Resolve(
              MakeGarbageCollected<NativeFileSystemDirectoryHandle>(
                  kSandboxRootDirectoryName,
                  RevocableInterfacePtr<
                      mojom::blink::NativeFileSystemDirectoryHandle>(
                      handle.PassInterface(),
                      context->GetInterfaceInvalidator(),
                      context->GetTaskRunner(TaskType::kMiscPlatformAPI))));
        } else {
          resolver->Reject(file_error::CreateDOMException(result->error_code));
        }
      },
      WrapPersistent(resolver), std::move(manager)));

  return result;
}

mojom::blink::NativeFileSystemTransferTokenPtr
NativeFileSystemDirectoryHandle::Transfer() {
  mojom::blink::NativeFileSystemTransferTokenPtr result;
  mojo_ptr_->Transfer(mojo::MakeRequest(&result));
  return result;
}

void NativeFileSystemDirectoryHandle::QueryPermissionImpl(
    bool writable,
    base::OnceCallback<void(mojom::blink::PermissionStatus)> callback) {
  mojo_ptr_->GetPermissionStatus(writable, std::move(callback));
}

void NativeFileSystemDirectoryHandle::RequestPermissionImpl(
    bool writable,
    base::OnceCallback<void(mojom::blink::PermissionStatus)> callback) {
  mojo_ptr_->RequestPermission(writable, std::move(callback));
}

}  // namespace blink
