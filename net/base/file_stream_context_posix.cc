// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream_context.h"

#include <dlfcn.h>
#include <errno.h>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

#if BUILDFLAG(IS_MAC)
#include "net/base/apple/guarded_fd.h"
#endif  // BUILDFLAG(IS_MAC)

namespace net {

FileStream::Context::Context(scoped_refptr<base::TaskRunner> task_runner)
    : Context(base::File(), std::move(task_runner)) {}

FileStream::Context::Context(base::File file,
                             scoped_refptr<base::TaskRunner> task_runner)
    : file_(std::move(file)), task_runner_(std::move(task_runner)) {
#if BUILDFLAG(IS_MAC)
  // https://crbug.com/330771755: Guard against a file descriptor being closed
  // out from underneath the file.
  typedef int (*change_fdguard_np_ptr_t)(
      int fd, const guardid_t* guard, u_int guardflags, const guardid_t* nguard,
      u_int nguardflags, int* fdflagsp);
  static const change_fdguard_np_ptr_t
      change_fdguard_np_ptr = reinterpret_cast<change_fdguard_np_ptr_t>(
          dlsym(((void*)-2), "change_fdguard_np"));
  if (change_fdguard_np_ptr) {
      if (file_.IsValid()) {
        guardid_t guardid = reinterpret_cast<guardid_t>(this);
        PCHECK(change_fdguard_np_ptr(file_.GetPlatformFile(), /*guard=*/nullptr,
                                 /*guardflags=*/0, &guardid,
                                 GUARD_CLOSE | GUARD_DUP,
                                 /*fdflagsp=*/nullptr) == 0);
      }
    }
#endif
}

FileStream::Context::~Context() = default;

int FileStream::Context::Read(IOBuffer* in_buf,
                              int buf_len,
                              CompletionOnceCallback callback) {
  DCHECK(!async_in_progress_);

  scoped_refptr<IOBuffer> buf = in_buf;
  const bool posted = task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&Context::ReadFileImpl, base::Unretained(this), buf,
                     buf_len),
      base::BindOnce(&Context::OnAsyncCompleted, base::Unretained(this),
                     IntToInt64(std::move(callback))));
  DCHECK(posted);

  async_in_progress_ = true;
  return ERR_IO_PENDING;
}

int FileStream::Context::Write(IOBuffer* in_buf,
                               int buf_len,
                               CompletionOnceCallback callback) {
  DCHECK(!async_in_progress_);

  scoped_refptr<IOBuffer> buf = in_buf;
  const bool posted = task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&Context::WriteFileImpl, base::Unretained(this), buf,
                     buf_len),
      base::BindOnce(&Context::OnAsyncCompleted, base::Unretained(this),
                     IntToInt64(std::move(callback))));
  DCHECK(posted);

  async_in_progress_ = true;
  return ERR_IO_PENDING;
}

FileStream::Context::IOResult FileStream::Context::SeekFileImpl(
    int64_t offset) {
  int64_t res = file_.Seek(base::File::FROM_BEGIN, offset);
  if (res == -1)
    return IOResult::FromOSError(errno);

  return IOResult(res, 0);
}

void FileStream::Context::OnFileOpened() {
}

FileStream::Context::IOResult FileStream::Context::ReadFileImpl(
    scoped_refptr<IOBuffer> buf,
    int buf_len) {
  int res = file_.ReadAtCurrentPosNoBestEffort(buf->data(), buf_len);
  if (res == -1)
    return IOResult::FromOSError(errno);

  return IOResult(res, 0);
}

FileStream::Context::IOResult FileStream::Context::WriteFileImpl(
    scoped_refptr<IOBuffer> buf,
    int buf_len) {
  int res = file_.WriteAtCurrentPosNoBestEffort(buf->data(), buf_len);
  if (res == -1)
    return IOResult::FromOSError(errno);

  return IOResult(res, 0);
}

}  // namespace net
