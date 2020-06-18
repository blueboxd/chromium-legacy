// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor_archive_minizip.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor_io_javascript_stream.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor_stream.h"
#include "third_party/minizip/src/mz.h"
#include "third_party/minizip/src/mz_strm.h"
#include "third_party/minizip/src/mz_zip.h"

namespace {

const char kCreateArchiveError[] = "Failed to create archive.";
const char kAddToArchiveError[] = "Failed to add entry to archive.";
const char kCloseArchiveError[] = "Failed to close archive.";

// We need at least 256KB for MiniZip.
const int64_t kMaximumDataChunkSize = 512 * 1024;

int32_t MinizipIsOpen(void* stream) {
  // The stream is always in an open state since it's tied to the life of the
  // CompressorArchiveMinizip instance.
  return MZ_OK;
}

}  // namespace

// vtable for the archive write stream provided to minizip. Only functions which
// are necessary for compression are implemented.
mz_stream_vtbl CompressorArchiveMinizip::minizip_vtable = {
    nullptr,                                          /* open */
    MinizipIsOpen, nullptr,                           /* read */
    MinizipWrite,  MinizipTell, MinizipSeek, nullptr, /* close */
    nullptr,                                          /* error */
    nullptr,                                          /* create */
    nullptr,                                          /* destroy */
    nullptr,                                          /* get_prop_int64 */
    nullptr                                           /* set_prop_int64 */
};

// Stream object used by minizip to access archive files.
struct CompressorArchiveMinizip::MinizipStream {
  // Must be the first element because minizip internally will cast this
  // struct into a mz_stream.
  mz_stream stream_ = {
      &minizip_vtable, nullptr,
  };

  CompressorArchiveMinizip* archive_;

  explicit MinizipStream(CompressorArchiveMinizip* archive)
      : archive_(archive) {
    DCHECK(archive);
  }
};

// Called when data chunk must be written on the archive. It copies data
// from the given buffer processed by minizip to an array buffer and passes
// it to compressor_stream.
int32_t CompressorArchiveMinizip::MinizipWrite(void* stream,
                                               const void* buffer,
                                               int32_t length) {
  return static_cast<MinizipStream*>(stream)->archive_->StreamWrite(buffer,
                                                                    length);
}

int32_t CompressorArchiveMinizip::StreamWrite(const void* buffer,
                                              int32_t write_size) {
  int64_t written_bytes = compressor_stream()->Write(
      offset_, write_size, static_cast<const char*>(buffer));

  if (written_bytes != write_size)
    return MZ_STREAM_ERROR;

  // Update offset_ and length_.
  offset_ += written_bytes;
  if (offset_ > length_)
    length_ = offset_;
  return write_size;
}

// Returns the offset from the beginning of the data.
int64_t CompressorArchiveMinizip::MinizipTell(void* stream) {
  return static_cast<MinizipStream*>(stream)->archive_->StreamTell();
}

int64_t CompressorArchiveMinizip::StreamTell() {
  return offset_;
}

// Moves the current offset to the specified position.
int32_t CompressorArchiveMinizip::MinizipSeek(void* stream,
                                              int64_t offset,
                                              int32_t origin) {
  return static_cast<MinizipStream*>(stream)->archive_->StreamSeek(offset,
                                                                   origin);
}

int32_t CompressorArchiveMinizip::StreamSeek(int64_t offset, int32_t origin) {
  switch (origin) {
    case MZ_SEEK_SET:
      offset_ = std::min(offset, length_);
      break;
    case MZ_SEEK_CUR:
      offset_ = std::min(offset_ + offset, length_);
      break;
    case MZ_SEEK_END:
      offset_ = std::max(length_ + offset, static_cast<int64_t>(0));
      break;
    default:
      NOTREACHED();
      return MZ_STREAM_ERROR;
  }

  return MZ_OK;
}

CompressorArchiveMinizip::CompressorArchiveMinizip(
    CompressorStream* compressor_stream)
    : CompressorArchive(compressor_stream),
      compressor_stream_(compressor_stream),
      stream_(std::make_unique<MinizipStream>(this)),
      zip_file_(nullptr),
      destination_buffer_(std::make_unique<char[]>(kMaximumDataChunkSize)),
      offset_(0),
      length_(0) {
  static_assert(offsetof(CompressorArchiveMinizip::MinizipStream, stream_) == 0,
                "Bad mz_stream offset");
}

CompressorArchiveMinizip::~CompressorArchiveMinizip() = default;

bool CompressorArchiveMinizip::CreateArchive() {
  zip_file_.reset(mz_zip_create(nullptr));
  int32_t result = mz_zip_open(zip_file_.get(), stream_.get(),
                               MZ_OPEN_MODE_WRITE | MZ_OPEN_MODE_CREATE);
  if (result != MZ_OK) {
    set_error_message(kCreateArchiveError);
    return false;
  }
  return true /* Success */;
}

bool CompressorArchiveMinizip::AddToArchive(const std::string& filename,
                                            int64_t file_size,
                                            base::Time modification_time,
                                            bool is_directory) {
  // Minizip takes filenames that end with '/' as directories.
  std::string normalized_filename = filename;
  if (is_directory) {
    normalized_filename += "/";
    // For a directory, some file systems much give us a non-zero file size
    // (i.e. 4096, page size). Normalize the size of a directory to zero.
    file_size = 0;
  }

  mz_zip_file file_info = {};
  file_info.creation_date = modification_time.ToTimeT();
  file_info.modified_date = file_info.creation_date;
  file_info.accessed_date = file_info.creation_date;
  file_info.uncompressed_size = file_size;
  file_info.filename = normalized_filename.c_str();
  file_info.filename_size = normalized_filename.length();
  file_info.flag = MZ_ZIP_FLAG_UTF8;

  // PKWARE .ZIP File Format Specification version 6.3.x
  const uint16_t ZIP_SPECIFICATION_VERSION_CODE = 63;
  file_info.version_madeby =
      MZ_HOST_SYSTEM_UNIX << 8 | ZIP_SPECIFICATION_VERSION_CODE;

  // Compression options.
  file_info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
  file_info.zip64 = MZ_ZIP64_AUTO;

  int32_t open_result = mz_zip_entry_write_open(
      zip_file_.get(), &file_info, MZ_COMPRESS_LEVEL_DEFAULT, 0 /* raw */,
      nullptr /* password */);
  if (open_result != MZ_OK) {
    LOG(ERROR) << "Archive creation error " << open_result;
    CloseArchive(true /* has_error */);
    set_error_message(kAddToArchiveError);
    return false /* Error */;
  }

  bool has_error = false;
  if (!is_directory) {
    int64_t remaining_size = file_size;
    while (remaining_size > 0) {
      int64_t chunk_size = std::min(remaining_size, kMaximumDataChunkSize);
      DCHECK_GT(chunk_size, 0);

      int64_t read_bytes =
          compressor_stream_->Read(chunk_size, destination_buffer_.get());
      // Negative read_bytes indicates an error occurred when reading chunks.
      // 0 just means there is no more data available, but here we need positive
      // length of bytes, so this is also an error here.
      if (read_bytes <= 0) {
        has_error = true;
        break;
      }

      if (canceled_) {
        break;
      }

      int32_t write_result = mz_zip_entry_write(
          zip_file_.get(), destination_buffer_.get(), read_bytes);
      if (write_result != read_bytes) {
        LOG(ERROR) << "Zip entry write error " << write_result;
        has_error = true;
        break;
      }
      remaining_size -= read_bytes;
    }
  }

  int32_t close_result = mz_zip_entry_close(zip_file_.get());
  if (close_result != MZ_OK) {
    LOG(ERROR) << "Zip entry close error " << close_result;
    has_error = true;
  }

  if (has_error) {
    CloseArchive(true /* has_error */);
    set_error_message(kAddToArchiveError);
    return false /* Error */;
  }

  if (canceled_) {
    CloseArchive(true /* has_error */);
    return false /* Error */;
  }

  return true /* Success */;
}

bool CompressorArchiveMinizip::CloseArchive(bool has_error) {
  int32_t result = mz_zip_close(zip_file_.get());
  if (result != MZ_OK) {
    set_error_message(kCloseArchiveError);
    return false /* Error */;
  }
  if (!has_error) {
    if (compressor_stream()->Flush() < 0) {
      set_error_message(kCloseArchiveError);
      return false /* Error */;
    }
  }
  return true /* Success */;
}

void CompressorArchiveMinizip::CancelArchive() {
  canceled_ = true;
}
