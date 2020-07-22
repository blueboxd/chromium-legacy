// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_file_writer_impl.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/services/quarantine/quarantine.h"
#include "content/browser/native_file_system/native_file_system_error.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "crypto/secure_hash.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom.h"

using blink::mojom::NativeFileSystemStatus;
using storage::BlobDataHandle;
using storage::FileSystemOperation;
using storage::FileSystemOperationRunner;

namespace {

// For after write checks we need the hash and size of the file. That data is
// calculated on a worker thread, and this struct is used to pass it back.
struct HashResult {
  base::File::Error status;
  // SHA256 hash of the file contents, an empty string if some error occurred.
  std::string hash;
  // Can be -1 to indicate an error calculating the hash and/or size.
  int64_t file_size = -1;
};

HashResult ReadAndComputeSHA256ChecksumAndSize(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!file.IsValid())
    return {file.error_details(), std::string(), -1};

  std::unique_ptr<crypto::SecureHash> hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  std::vector<char> buffer(8 * 1024);
  int bytes_read = file.ReadAtCurrentPos(buffer.data(), buffer.size());

  while (bytes_read > 0) {
    hash->Update(buffer.data(), bytes_read);
    bytes_read = file.ReadAtCurrentPos(buffer.data(), buffer.size());
  }

  // If bytes_read is -ve, it means there were issues reading from disk.
  if (bytes_read < 0)
    return {file.error_details(), std::string(), -1};

  std::string hash_str(hash->GetHashLength(), 0);
  hash->Finish(base::data(hash_str), hash_str.size());

  return {file.error_details(), hash_str, file.GetLength()};
}

}  // namespace

namespace content {

struct NativeFileSystemFileWriterImpl::WriteState {
  WriteCallback callback;
  uint64_t bytes_written = 0;
};

NativeFileSystemFileWriterImpl::NativeFileSystemFileWriterImpl(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const storage::FileSystemURL& swap_url,
    const SharedHandleState& handle_state,
    bool has_transient_user_activation,
    download::QuarantineConnectionCallback quarantine_connection_callback)
    : NativeFileSystemHandleBase(manager, context, url, handle_state),
      swap_url_(swap_url),
      quarantine_connection_callback_(
          std::move(quarantine_connection_callback)),
      has_transient_user_activation_(has_transient_user_activation) {
  DCHECK_EQ(swap_url.type(), url.type());
}

NativeFileSystemFileWriterImpl::~NativeFileSystemFileWriterImpl() {
  if (can_purge()) {
    DoFileSystemOperation(FROM_HERE, &FileSystemOperationRunner::RemoveFile,
                          base::BindOnce(
                              [](const storage::FileSystemURL& swap_url,
                                 base::File::Error result) {
                                if (result != base::File::FILE_OK) {
                                  DLOG(ERROR)
                                      << "Error Deleting Swap File, status: "
                                      << base::File::ErrorToString(result)
                                      << " path: " << swap_url.path();
                                }
                              },
                              swap_url()),
                          swap_url());
  }
}

void NativeFileSystemFileWriterImpl::Write(
    uint64_t offset,
    mojo::PendingRemote<blink::mojom::Blob> data,
    WriteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::WriteImpl,
                     weak_factory_.GetWeakPtr(), offset, std::move(data)),
      base::BindOnce([](WriteCallback callback) {
        std::move(callback).Run(native_file_system_error::FromStatus(
                                    NativeFileSystemStatus::kPermissionDenied),
                                /*bytes_written=*/0);
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::WriteStream(
    uint64_t offset,
    mojo::ScopedDataPipeConsumerHandle stream,
    WriteStreamCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::WriteStreamImpl,
                     weak_factory_.GetWeakPtr(), offset, std::move(stream)),
      base::BindOnce([](WriteStreamCallback callback) {
        std::move(callback).Run(native_file_system_error::FromStatus(
                                    NativeFileSystemStatus::kPermissionDenied),
                                /*bytes_written=*/0);
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::Truncate(uint64_t length,
                                              TruncateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::TruncateImpl,
                     weak_factory_.GetWeakPtr(), length),
      base::BindOnce([](TruncateCallback callback) {
        std::move(callback).Run(native_file_system_error::FromStatus(
            NativeFileSystemStatus::kPermissionDenied));
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::Close(CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::CloseImpl,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce([](CloseCallback callback) {
        std::move(callback).Run(native_file_system_error::FromStatus(
            NativeFileSystemStatus::kPermissionDenied));
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::WriteImpl(
    uint64_t offset,
    mojo::PendingRemote<blink::mojom::Blob> data,
    WriteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  if (is_closed()) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kInvalidState,
            "An attempt was made to write to a closed writer."),
        /*bytes_written=*/0);
    return;
  }

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      blink::BlobUtils::GetDataPipeCapacity(blink::BlobUtils::kUnknownSize);

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult rv =
      mojo::CreateDataPipe(&options, &producer_handle, &consumer_handle);
  if (rv != MOJO_RESULT_OK) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kOperationFailed,
            "Internal read error: failed to create mojo data pipe."),
        /*bytes_written=*/0);
    return;
  }

  // TODO(mek): We can do this transformation from Blob to DataPipe in the
  // renderer, and simplify the mojom exposed interface.
  mojo::Remote<blink::mojom::Blob> blob(std::move(data));
  blob->ReadAll(std::move(producer_handle), mojo::NullRemote());
  WriteStreamImpl(offset, std::move(consumer_handle), std::move(callback));
}

void NativeFileSystemFileWriterImpl::WriteStreamImpl(
    uint64_t offset,
    mojo::ScopedDataPipeConsumerHandle stream,
    WriteStreamCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  if (is_closed()) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kInvalidState,
            "An attempt was made to write to a closed writer."),
        /*bytes_written=*/0);
    return;
  }

  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::WriteStream,
      base::BindRepeating(&NativeFileSystemFileWriterImpl::DidWrite,
                          weak_factory_.GetWeakPtr(),
                          base::Owned(new WriteState{std::move(callback)})),
      swap_url(), std::move(stream), offset);
}

void NativeFileSystemFileWriterImpl::DidWrite(WriteState* state,
                                              base::File::Error result,
                                              int64_t bytes,
                                              bool complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(state);
  state->bytes_written += bytes;
  if (complete) {
    std::move(state->callback)
        .Run(native_file_system_error::FromFileError(result),
             state->bytes_written);
  }
}

void NativeFileSystemFileWriterImpl::TruncateImpl(uint64_t length,
                                                  TruncateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  if (is_closed()) {
    std::move(callback).Run(native_file_system_error::FromStatus(
        NativeFileSystemStatus::kInvalidState,
        "An attempt was made to write to a closed writer."));
    return;
  }

  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::Truncate,
      base::BindOnce(
          [](TruncateCallback callback, base::File::Error result) {
            std::move(callback).Run(
                native_file_system_error::FromFileError(result));
          },
          std::move(callback)),
      swap_url(), length);
}

void NativeFileSystemFileWriterImpl::CloseImpl(CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);
  if (is_closed()) {
    std::move(callback).Run(native_file_system_error::FromStatus(
        NativeFileSystemStatus::kInvalidState,
        "An attempt was made to close an already closed writer."));
    return;
  }

  // Should the writer be destructed at this point, we want to allow the
  // close operation to run its course, so we should not purge the swap file.
  // If the after write check fails, the callback for that will clean up the
  // swap file even if the writer was destroyed at that point.
  state_ = State::kClosePending;

  if (!RequireSecurityChecks() || !manager()->permission_context()) {
    DidPassAfterWriteCheck(std::move(callback));
    return;
  }

  ComputeHashForSwapFile(base::BindOnce(
      &NativeFileSystemFileWriterImpl::DoAfterWriteCheck,
      weak_factory_.GetWeakPtr(), swap_url().path(), std::move(callback)));
}

// static
void NativeFileSystemFileWriterImpl::DoAfterWriteCheck(
    base::WeakPtr<NativeFileSystemFileWriterImpl> file_writer,
    const base::FilePath& swap_path,
    NativeFileSystemFileWriterImpl::CloseCallback callback,
    base::File::Error hash_result,
    const std::string& hash,
    int64_t size) {
  if (!file_writer || hash_result != base::File::FILE_OK) {
    // If writer was deleted, or calculating the hash failed try deleting the
    // swap file and invoke the callback.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(base::GetDeleteFileCallback(), swap_path));
    std::move(callback).Run(native_file_system_error::FromStatus(
        NativeFileSystemStatus::kOperationAborted,
        "Failed to perform Safe Browsing check."));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(file_writer->sequence_checker_);

  auto item = std::make_unique<NativeFileSystemWriteItem>();
  item->target_file_path = file_writer->url().path();
  item->full_path = file_writer->swap_url().path();
  item->sha256_hash = hash;
  item->size = size;
  item->frame_url = file_writer->context().url;
  item->has_user_gesture = file_writer->has_transient_user_activation_;
  file_writer->manager()->permission_context()->PerformAfterWriteChecks(
      std::move(item), file_writer->context().frame_id,
      base::BindOnce(&NativeFileSystemFileWriterImpl::DidAfterWriteCheck,
                     file_writer, swap_path, std::move(callback)));
}

// static
void NativeFileSystemFileWriterImpl::DidAfterWriteCheck(
    base::WeakPtr<NativeFileSystemFileWriterImpl> file_writer,
    const base::FilePath& swap_path,
    NativeFileSystemFileWriterImpl::CloseCallback callback,
    NativeFileSystemPermissionContext::AfterWriteCheckResult result) {
  if (file_writer &&
      result ==
          NativeFileSystemPermissionContext::AfterWriteCheckResult::kAllow) {
    file_writer->DidPassAfterWriteCheck(std::move(callback));
    return;
  }

  // Writer is gone, or safe browsing check failed. In this case we should
  // try deleting the swap file and call the callback to report that close
  // failed.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(base::GetDeleteFileCallback(), swap_path));
  std::move(callback).Run(native_file_system_error::FromStatus(
      NativeFileSystemStatus::kOperationAborted,
      "Write operation blocked by Safe Browsing."));
  return;
}

void NativeFileSystemFileWriterImpl::DidPassAfterWriteCheck(
    CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the move operation succeeds, the path pointing to the swap file
  // will not exist anymore.
  // In case of error, the swap file URL will point to a valid filesystem
  // location. The file at this URL will be deleted when the mojo pipe closes.
  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::Move,
      base::BindOnce(&NativeFileSystemFileWriterImpl::DidSwapFileBeforeClose,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      swap_url(), url(),
      storage::FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED);
}

void NativeFileSystemFileWriterImpl::DidSwapFileBeforeClose(
    CloseCallback callback,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != base::File::FILE_OK) {
    state_ = State::kCloseError;
    DLOG(ERROR) << "Swap file move operation failed source: "
                << swap_url().path() << " dest: " << url().path()
                << " error: " << base::File::ErrorToString(result);
    std::move(callback).Run(native_file_system_error::FromFileError(result));
    return;
  }

  if (!RequireSecurityChecks()) {
    state_ = State::kClosed;
    std::move(callback).Run(native_file_system_error::Ok());
    return;
  }

  GURL referrer_url = manager()->is_off_the_record() ? GURL() : context().url;
  GURL authority_url =
      referrer_url.is_valid() && referrer_url.SchemeIsHTTPOrHTTPS()
          ? referrer_url
          : GURL();

  mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote;
  if (quarantine_connection_callback_) {
    quarantine_connection_callback_.Run(
        quarantine_remote.BindNewPipeAndPassReceiver());
    quarantine::mojom::Quarantine* raw_quarantine = quarantine_remote.get();
    raw_quarantine->QuarantineFile(
        url().path(), authority_url, referrer_url,
        GetContentClient()
            ->browser()
            ->GetApplicationClientGUIDForQuarantineCheck(),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&NativeFileSystemFileWriterImpl::DidAnnotateFile,
                           weak_factory_.GetWeakPtr(), std::move(callback),
                           std::move(quarantine_remote)),
            quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED));
  } else {
#if defined(OS_WIN)
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&quarantine::SetInternetZoneIdentifierDirectly,
                       url().path(), authority_url, referrer_url),
        base::BindOnce(&NativeFileSystemFileWriterImpl::DidAnnotateFile,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(quarantine_remote)));
#else
    DidAnnotateFile(std::move(callback), std::move(quarantine_remote),
                    quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED);
#endif
  }
}

void NativeFileSystemFileWriterImpl::DidAnnotateFile(
    CloseCallback callback,
    mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
    quarantine::mojom::QuarantineFileResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kClosed;

  if (result != quarantine::mojom::QuarantineFileResult::OK &&
      result != quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED) {
    // If malware was detected, or the file referrer was blocked by policy, the
    // file will be deleted at this point by AttachmentServices on Windows.
    // There is nothing to do except to return the error message to the
    // application.
    std::move(callback).Run(native_file_system_error::FromStatus(
        NativeFileSystemStatus::kOperationAborted,
        "Write operation aborted due to security policy."));
    return;
  }

  std::move(callback).Run(native_file_system_error::Ok());
}

void NativeFileSystemFileWriterImpl::ComputeHashForSwapFile(
    HashCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if defined(OS_CHROMEOS)
  // TOOD(crbug.com/1103076): Extend this check to non-native paths.
  DCHECK(swap_url().type() == storage::kFileSystemTypeNativeLocal ||
         swap_url().type() == storage::kFileSystemTypeNativeForPlatformApp)
      << swap_url().type();
#else
  DCHECK_EQ(swap_url().type(), storage::kFileSystemTypeNativeLocal);
#endif

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadAndComputeSHA256ChecksumAndSize, swap_url().path()),
      base::BindOnce(
          [](HashCallback callback, HashResult result) {
            std::move(callback).Run(result.status, result.hash,
                                    result.file_size);
          },
          std::move(callback)));
}

base::WeakPtr<NativeFileSystemHandleBase>
NativeFileSystemFileWriterImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
