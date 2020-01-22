// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/lzma_util.h"

#include <windows.h>

#include <stddef.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/installer/util/lzma_file_allocator.h"

extern "C" {
#include "third_party/lzma_sdk/7z.h"
#include "third_party/lzma_sdk/7zAlloc.h"
#include "third_party/lzma_sdk/7zCrc.h"
#include "third_party/lzma_sdk/7zFile.h"
}

namespace {

SRes LzmaReadFile(HANDLE file, void* data, size_t* size) {
  if (*size == 0)
    return SZ_OK;

  size_t processedSize = 0;
  DWORD maxSize = *size;
  do {
    DWORD processedLoc = 0;
    BOOL res = ReadFile(file, data, maxSize, &processedLoc, NULL);
    data = (void*)((unsigned char*)data + processedLoc);
    maxSize -= processedLoc;
    processedSize += processedLoc;
    if (processedLoc == 0) {
      if (res)
        return SZ_ERROR_READ;
      else
        break;
    }
  } while (maxSize > 0);

  *size = processedSize;
  return SZ_OK;
}

SRes SzFileSeekImp(void* object, Int64* pos, ESzSeek origin) {
  CFileInStream* s = (CFileInStream*)object;
  LARGE_INTEGER value;
  value.LowPart = (DWORD)*pos;
  value.HighPart = (LONG)((UInt64)*pos >> 32);
  DWORD moveMethod;
  switch (origin) {
    case SZ_SEEK_SET:
      moveMethod = FILE_BEGIN;
      break;
    case SZ_SEEK_CUR:
      moveMethod = FILE_CURRENT;
      break;
    case SZ_SEEK_END:
      moveMethod = FILE_END;
      break;
    default:
      return SZ_ERROR_PARAM;
  }
  value.LowPart = SetFilePointer(s->file.handle, value.LowPart, &value.HighPart,
                                 moveMethod);
  *pos = ((Int64)value.HighPart << 32) | value.LowPart;
  return ((value.LowPart == 0xFFFFFFFF) && (GetLastError() != ERROR_SUCCESS))
             ? SZ_ERROR_FAIL
             : SZ_OK;
}

SRes SzFileReadImp(void* object, void* buffer, size_t* size) {
  CFileInStream* s = (CFileInStream*)object;
  return LzmaReadFile(s->file.handle, buffer, size);
}

// Returns EXCEPTION_EXECUTE_HANDLER and populates |status| with the underlying
// NTSTATUS code for paging errors encountered while accessing file-backed
// mapped memory. Otherwise, return EXCEPTION_CONTINUE_SEARCH.
DWORD FilterPageError(const LzmaFileAllocator& file_allocator,
                      DWORD exception_code,
                      const EXCEPTION_POINTERS* info,
                      int32_t* status) {
  if (exception_code != EXCEPTION_IN_PAGE_ERROR)
    return EXCEPTION_CONTINUE_SEARCH;

  const EXCEPTION_RECORD* exception_record = info->ExceptionRecord;
  if (!file_allocator.IsAddressMapped(
          exception_record->ExceptionInformation[1])) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  // Cast NTSTATUS to int32_t to avoid including winternl.h
  *status = exception_record->ExceptionInformation[2];

  return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

UnPackStatus UnPackArchive(const base::FilePath& archive,
                           const base::FilePath& output_dir,
                           base::FilePath* output_file,
                           base::Optional<DWORD>* error_code,
                           base::Optional<int32_t>* ntstatus) {
  VLOG(1) << "Opening archive " << archive.value();
  LzmaUtilImpl lzma_util;
  UnPackStatus status;
  if ((status = lzma_util.OpenArchive(archive)) != UNPACK_NO_ERROR) {
    PLOG(ERROR) << "Unable to open install archive: " << archive.value();
  } else {
    VLOG(1) << "Uncompressing archive to path " << output_dir.value();
    if ((status = lzma_util.UnPack(output_dir, output_file)) != UNPACK_NO_ERROR)
      PLOG(ERROR) << "Unable to uncompress archive: " << archive.value();
  }
  if (error_code)
    *error_code = lzma_util.GetErrorCode();
  if (ntstatus)
    *ntstatus = lzma_util.GetNTSTATUSCode();
  return status;
}

LzmaUtilImpl::LzmaUtilImpl() = default;
LzmaUtilImpl::~LzmaUtilImpl() = default;

UnPackStatus LzmaUtilImpl::OpenArchive(const base::FilePath& archivePath) {
  // Make sure file is not already open.
  CloseArchive();

  archive_file_.Initialize(archivePath, base::File::FLAG_OPEN |
                                            base::File::FLAG_READ |
                                            base::File::FLAG_EXCLUSIVE_WRITE);
  if (archive_file_.IsValid())
    return UNPACK_NO_ERROR;
  error_code_ = ::GetLastError();
  return archive_file_.error_details() == base::File::FILE_ERROR_NOT_FOUND
             ? UNPACK_ARCHIVE_NOT_FOUND
             : UNPACK_ARCHIVE_CANNOT_OPEN;
}

UnPackStatus LzmaUtilImpl::UnPack(const base::FilePath& location) {
  return UnPack(location, NULL);
}

UnPackStatus LzmaUtilImpl::UnPack(const base::FilePath& location,
                                  base::FilePath* output_file) {
  DCHECK(archive_file_.IsValid());

  CFileInStream archiveStream;
  CLookToRead lookStream;
  CSzArEx db;
  ISzAlloc allocImp;
  ISzAlloc allocTempImp;
  SRes sz_res = SZ_OK;

  archiveStream.file.handle = archive_file_.GetPlatformFile();
  archiveStream.s.Read = SzFileReadImp;
  archiveStream.s.Seek = SzFileSeekImp;
  LookToRead_CreateVTable(&lookStream, false);
  lookStream.realStream = &archiveStream.s;

  allocImp.Alloc = SzAlloc;
  allocImp.Free = SzFree;
  allocTempImp.Alloc = SzAllocTemp;
  allocTempImp.Free = SzFreeTemp;

  CrcGenerateTable();
  SzArEx_Init(&db);
  ::SetLastError(ERROR_SUCCESS);
  if ((sz_res = SzArEx_Open(&db, &lookStream.s, &allocImp, &allocTempImp)) !=
      SZ_OK) {
    LOG(ERROR) << L"Error returned by SzArchiveOpen: " << sz_res;
    auto error_code = ::GetLastError();
    if (error_code != ERROR_SUCCESS)
      error_code_ = error_code;
    return UNPACK_SZAREX_OPEN_ERROR;
  }

  Byte* outBuffer = 0;  // it must be 0 before first call for each new archive
  UInt32 blockIndex = 0xFFFFFFFF;  // can have any value if outBuffer = 0
  size_t outBufferSize = 0;        // can have any value if outBuffer = 0

  LzmaFileAllocator fileAllocator(location);

  UnPackStatus status = UNPACK_NO_ERROR;
  for (unsigned int i = 0; i < db.NumFiles; i++) {
    DWORD written;
    size_t offset = 0;
    size_t outSizeProcessed = 0;

    int32_t ntstatus = 0;  // STATUS_SUCCESS
    ::SetLastError(ERROR_SUCCESS);
    __try {
      if ((sz_res =
               SzArEx_Extract(&db, &lookStream.s, i, &blockIndex, &outBuffer,
                              &outBufferSize, &offset, &outSizeProcessed,
                              &fileAllocator, &allocTempImp)) != SZ_OK) {
        LOG(ERROR) << L"Error returned by SzExtract: " << sz_res;
        auto error_code = ::GetLastError();
        if (error_code != ERROR_SUCCESS)
          error_code_ = error_code;
        status = UNPACK_EXTRACT_ERROR;
      }
    } __except(FilterPageError(fileAllocator, GetExceptionCode(),
                                GetExceptionInformation(), &ntstatus)) {
      ntstatus_ = ntstatus;
      status = UNPACK_EXTRACT_EXCEPTION;
      LOG(ERROR) << L"EXCEPTION_IN_PAGE_ERROR while accessing mapped memory; "
                    L"NTSTATUS = "
                 << ntstatus;
    }
    if (status != UNPACK_NO_ERROR)
      break;

    size_t file_name_length = SzArEx_GetFileNameUtf16(&db, i, NULL);
    if (file_name_length < 1) {
      LOG(ERROR) << L"Couldn't get file name";
      status = UNPACK_NO_FILENAME_ERROR;
      break;
    }

    std::vector<UInt16> file_name(file_name_length);
    SzArEx_GetFileNameUtf16(&db, i, &file_name[0]);
    // |file_name| is NULL-terminated.
    base::FilePath file_path = location.Append(
        base::FilePath::StringType(file_name.begin(), file_name.end() - 1));

    if (output_file)
      *output_file = file_path;

    // If archive entry is directory create it and move on to the next entry.
    if (SzArEx_IsDir(&db, i)) {
      if (!CreateDirectory(file_path)) {
        error_code_ = ::GetLastError();
        status = UNPACK_CREATE_FILE_ERROR;
        break;
      }
      continue;
    }

    CreateDirectory(file_path.DirName());

    HANDLE hFile;
    hFile = CreateFile(file_path.value().c_str(), GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
      error_code_ = ::GetLastError();
      status = UNPACK_CREATE_FILE_ERROR;
      PLOG(ERROR) << L"CreateFile failed";
      break;
    }

    // Don't write all of the data at once because this can lead to kernel
    // address-space exhaustion on 32-bit Windows (see https://crbug.com/1001022
    // for details).
    constexpr size_t kMaxWriteAmount = 8 * 1024 * 1024;
    for (size_t total_written = 0; total_written < outSizeProcessed; /**/) {
      const size_t write_amount =
          std::min(kMaxWriteAmount, outSizeProcessed - total_written);
      if ((!WriteFile(hFile, outBuffer + offset + total_written,
                      static_cast<DWORD>(write_amount), &written, nullptr)) ||
          (written != write_amount)) {
        error_code_ = ::GetLastError();
        status = UNPACK_WRITE_FILE_ERROR;
        PLOG(ERROR) << L"Error returned by WriteFile";
        CloseHandle(hFile);
        break;
      }
      total_written += write_amount;
    }

    // Break out of the file loop if the write loop failed.
    if (status != UNPACK_NO_ERROR)
      break;

    if (SzBitWithVals_Check(&db.MTime, i)) {
      if (!SetFileTime(hFile, NULL, NULL,
                       (const FILETIME*)(&db.MTime.Vals[i]))) {
        error_code_ = ::GetLastError();
        status = UNPACK_SET_FILE_TIME_ERROR;
        PLOG(ERROR) << L"Error returned by SetFileTime";
        CloseHandle(hFile);
        break;
      }
    }
    if (!CloseHandle(hFile)) {
      error_code_ = ::GetLastError();
      status = UNPACK_CLOSE_FILE_ERROR;
      PLOG(ERROR) << L"Error returned by CloseHandle";
      break;
    }
  }  // for loop
  IAlloc_Free(&fileAllocator, outBuffer);
  SzArEx_Free(&db, &allocImp);
  DCHECK(status != UNPACK_NO_ERROR || !error_code_.has_value());
  DCHECK(status != UNPACK_NO_ERROR || !ntstatus_.has_value());

  return status;
}

void LzmaUtilImpl::CloseArchive() {
  archive_file_.Close();
  error_code_ = base::nullopt;
  ntstatus_ = base::nullopt;
}

bool LzmaUtilImpl::CreateDirectory(const base::FilePath& dir) {
  bool result = true;
  if (directories_created_.find(dir) == directories_created_.end()) {
    result = base::CreateDirectory(dir);
    if (result)
      directories_created_.insert(dir);
  }
  return result;
}
