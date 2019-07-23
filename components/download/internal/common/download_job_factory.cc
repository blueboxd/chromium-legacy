// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_job_factory.h"

#include <memory>

#include "components/download/internal/common/download_job_impl.h"
#include "components/download/internal/common/parallel_download_job.h"
#include "components/download/internal/common/parallel_download_utils.h"
#include "components/download/internal/common/save_package_download_job.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_url_loader_factory_getter.h"
#include "net/url_request/url_request_context_getter.h"

namespace download {

namespace {
// Connection type of the download.
enum class ConnectionType {
  kHTTP = 0,
  kHTTP2,
  kQUIC,
  kUnknown,
};

ConnectionType GetConnectionType(
    net::HttpResponseInfo::ConnectionInfo connection_info) {
  switch (connection_info) {
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP0_9:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_0:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1:
      return ConnectionType::kHTTP;
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_14:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_15:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2:
      return ConnectionType::kHTTP2;
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_UNKNOWN_VERSION:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_32:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_33:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_34:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_35:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_36:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_37:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_38:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_39:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_40:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_41:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_42:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_43:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_44:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_45:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_46:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_47:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_48:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_99:
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC_999:
      return ConnectionType::kQUIC;
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
      return ConnectionType::kUnknown;
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      NOTREACHED();
      return ConnectionType::kUnknown;
  }
  NOTREACHED();
  return ConnectionType::kUnknown;
}

// Returns if the download can be parallelized.
bool IsParallelizableDownload(const DownloadCreateInfo& create_info,
                              DownloadItem* download_item) {
  // To enable parallel download, following conditions need to be satisfied.
  // 1. Feature |kParallelDownloading| enabled.
  // 2. Strong validators response headers. i.e. ETag and Last-Modified.
  // 3. Accept-Ranges or Content-Range header.
  // 4. Content-Length header.
  // 5. Content-Length is no less than the minimum slice size configuration, or
  // persisted slices alreay exist.
  // 6. HTTP/1.1 protocol, not QUIC nor HTTP/1.0.
  // 7. HTTP or HTTPS scheme with GET method in the initial request.

  // Etag and last modified are stored into DownloadCreateInfo in
  // DownloadRequestCore only if the response header complies to the strong
  // validator rule.
  bool has_strong_validator =
      !create_info.etag.empty() || !create_info.last_modified.empty();
  bool has_content_length = create_info.total_bytes > 0;
  bool satisfy_min_file_size =
      !download_item->GetReceivedSlices().empty() ||
      create_info.total_bytes >= GetMinSliceSizeConfig();
  ConnectionType type = GetConnectionType(create_info.connection_info);
  bool satisfy_connection_type =
      (create_info.connection_info ==
       net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1) ||
      (type == ConnectionType::kHTTP2 &&
       base::FeatureList::IsEnabled(features::kUseParallelRequestsForHTTP2)) ||
      (type == ConnectionType::kQUIC &&
       base::FeatureList::IsEnabled(features::kUseParallelRequestsForQUIC));
  bool http_get_method =
      create_info.method == "GET" && create_info.url().SchemeIsHTTPOrHTTPS();
  bool partial_response_success =
      download_item->GetReceivedSlices().empty() || create_info.offset != 0;
  bool range_support_allowed =
      create_info.accept_range == RangeRequestSupportType::kSupport ||
      (base::FeatureList::IsEnabled(
           features::kUseParallelRequestsForUnknwonRangeSupport) &&
       create_info.accept_range == RangeRequestSupportType::kUnknown);
  bool is_parallelizable = has_strong_validator && range_support_allowed &&
                           has_content_length && satisfy_min_file_size &&
                           satisfy_connection_type && http_get_method &&
                           partial_response_success;
  RecordDownloadConnectionInfo(create_info.connection_info);

  if (!IsParallelDownloadEnabled())
    return is_parallelizable;

  RecordParallelDownloadCreationEvent(
      is_parallelizable
          ? ParallelDownloadCreationEvent::STARTED_PARALLEL_DOWNLOAD
          : ParallelDownloadCreationEvent::FELL_BACK_TO_NORMAL_DOWNLOAD);
  if (!has_strong_validator) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_STRONG_VALIDATORS);
  }
  if (!range_support_allowed) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_ACCEPT_RANGE_HEADER);
    if (create_info.accept_range == RangeRequestSupportType::kUnknown) {
      RecordParallelDownloadCreationEvent(
          ParallelDownloadCreationEvent::FALLBACK_REASON_UNKNOWN_RANGE_SUPPORT);
    }
  }
  if (!has_content_length) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_CONTENT_LENGTH_HEADER);
  }
  if (!satisfy_min_file_size) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_FILE_SIZE);
  }
  if (!satisfy_connection_type) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_CONNECTION_TYPE);
  }
  if (!http_get_method) {
    RecordParallelDownloadCreationEvent(
        ParallelDownloadCreationEvent::FALLBACK_REASON_HTTP_METHOD);
  }

  return is_parallelizable;
}

}  // namespace

// static
std::unique_ptr<DownloadJob> DownloadJobFactory::CreateJob(
    DownloadItem* download_item,
    std::unique_ptr<DownloadRequestHandleInterface> req_handle,
    const DownloadCreateInfo& create_info,
    bool is_save_package_download,
    scoped_refptr<download::DownloadURLLoaderFactoryGetter>
        url_loader_factory_getter,
    net::URLRequestContextGetter* url_request_context_getter,
    service_manager::Connector* connector) {
  if (is_save_package_download) {
    return std::make_unique<SavePackageDownloadJob>(download_item,
                                                    std::move(req_handle));
  }

  bool is_parallelizable = IsParallelizableDownload(create_info, download_item);
  // Build parallel download job.
  if (IsParallelDownloadEnabled() && is_parallelizable) {
    return std::make_unique<ParallelDownloadJob>(
        download_item, std::move(req_handle), create_info,
        std::move(url_loader_factory_getter), url_request_context_getter,
        connector);
  }

  // An ordinary download job.
  return std::make_unique<DownloadJobImpl>(download_item, std::move(req_handle),
                                           is_parallelizable);
}

}  // namespace download
