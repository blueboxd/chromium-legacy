// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_new_script_loader.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "services/network/public/cpp/resource_response.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

namespace content {

// We chose this size because the AppCache uses this.
const uint32_t ServiceWorkerNewScriptLoader::kReadBufferSize = 32768;

// This is for debugging https://crbug.com/959627.
// The purpose is to see where the IOBuffer comes from by checking |__vfptr|.
class ServiceWorkerNewScriptLoader::WrappedIOBuffer
    : public net::WrappedIOBuffer {
 public:
  WrappedIOBuffer(const char* data) : net::WrappedIOBuffer(data) {}

 private:
  ~WrappedIOBuffer() override = default;

  // This is to make sure that the vtable is not merged with other classes.
  virtual void dummy() { NOTREACHED(); }
};

std::unique_ptr<ServiceWorkerNewScriptLoader>
ServiceWorkerNewScriptLoader::CreateForNetworkOnly(
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& original_request,
    network::mojom::URLLoaderClientPtr client,
    scoped_refptr<ServiceWorkerVersion> version,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  return base::WrapUnique(new ServiceWorkerNewScriptLoader(
      routing_id, request_id, options, original_request, std::move(client),
      version, loader_factory, traffic_annotation));
}

std::unique_ptr<ServiceWorkerNewScriptLoader>
ServiceWorkerNewScriptLoader::CreateForResume(
    uint32_t options,
    const network::ResourceRequest& original_request,
    network::mojom::URLLoaderClientPtr client,
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK(blink::ServiceWorkerUtils::IsImportedScriptUpdateCheckEnabled());
  return base::WrapUnique(new ServiceWorkerNewScriptLoader(
      options, original_request, std::move(client), version));
}

// TODO(nhiroki): We're doing multiple things in the ctor. Consider factors out
// some of them into a separate function.
ServiceWorkerNewScriptLoader::ServiceWorkerNewScriptLoader(
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& original_request,
    network::mojom::URLLoaderClientPtr client,
    scoped_refptr<ServiceWorkerVersion> version,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
    : request_url_(original_request.url),
      resource_type_(static_cast<ResourceType>(original_request.resource_type)),
      version_(version),
      network_client_binding_(this),
      network_watcher_(FROM_HERE,
                       mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                       base::SequencedTaskRunnerHandle::Get()),
      loader_factory_(std::move(loader_factory)),
      client_(std::move(client)),
      original_options_(options),
      type_(Type::kNetworkOnly),
      client_producer_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunnerHandle::Get()) {
  network::ResourceRequest resource_request(original_request);
#if DCHECK_IS_ON()
  CheckVersionStatusBeforeLoad();
#endif  // DCHECK_IS_ON()

  // TODO(nhiroki): Handle the case where |cache_resource_id| is invalid.
  int64_t cache_resource_id = version->context()->storage()->NewResourceId();

  // |incumbent_cache_resource_id| is valid if the incumbent service worker
  // exists and it's required to do the byte-for-byte check.
  int64_t incumbent_cache_resource_id =
      ServiceWorkerConsts::kInvalidServiceWorkerResourceId;
  scoped_refptr<ServiceWorkerRegistration> registration =
      version_->context()->GetLiveRegistration(version_->registration_id());
  // ServiceWorkerVersion keeps the registration alive while the service
  // worker is starting up, and it must be starting up here.
  DCHECK(registration);
  const bool is_main_script = resource_type_ == ResourceType::kServiceWorker;
  if (is_main_script) {
    ServiceWorkerVersion* stored_version = registration->waiting_version()
                                               ? registration->waiting_version()
                                               : registration->active_version();
    // |pause_after_download()| indicates the version is required to do the
    // byte-for-byte check.
    if (stored_version && stored_version->script_url() == request_url_ &&
        version_->pause_after_download()) {
      incumbent_cache_resource_id =
          stored_version->script_cache_map()->LookupResourceId(request_url_);
    }
    // Request SSLInfo. It will be persisted in service worker storage and
    // may be used by ServiceWorkerNavigationLoader for navigations handled
    // by this service worker.
    options |= network::mojom::kURLLoadOptionSendSSLInfoWithResponse;

    resource_request.headers.SetHeader("Service-Worker", "script");
  }

  // Validate the browser cache if needed, e.g., updateViaCache demands it or 24
  // hours passed since the last update check that hit network.
  base::TimeDelta time_since_last_check =
      base::Time::Now() - registration->last_update_check();
  if (service_worker_loader_helpers::ShouldValidateBrowserCacheForScript(
          is_main_script, version_->force_bypass_cache_for_scripts(),
          registration->update_via_cache(), time_since_last_check)) {
    resource_request.load_flags |= net::LOAD_VALIDATE_CACHE;
  }

  ServiceWorkerStorage* storage = version_->context()->storage();
  if (incumbent_cache_resource_id !=
      ServiceWorkerConsts::kInvalidServiceWorkerResourceId) {
    // Create response readers only when we have to do the byte-for-byte check.
    cache_writer_ = ServiceWorkerCacheWriter::CreateForComparison(
        storage->CreateResponseReader(incumbent_cache_resource_id),
        storage->CreateResponseReader(incumbent_cache_resource_id),
        storage->CreateResponseWriter(cache_resource_id),
        false /* pause_when_not_identical */);
  } else {
    // The script is new, create a cache writer for write back.
    cache_writer_ = ServiceWorkerCacheWriter::CreateForWriteBack(
        storage->CreateResponseWriter(cache_resource_id));
  }

  version_->script_cache_map()->NotifyStartedCaching(request_url_,
                                                     cache_resource_id);

  // Disable MIME sniffing. The spec requires the header list to have a
  // JavaScript MIME type. Therefore, no sniffing is needed.
  options &= ~network::mojom::kURLLoadOptionSniffMimeType;

  network::mojom::URLLoaderClientPtr network_client;
  network_client_binding_.Bind(mojo::MakeRequest(&network_client));
  loader_factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&network_loader_), routing_id, request_id, options,
      resource_request, std::move(network_client), traffic_annotation);
  DCHECK_EQ(NetworkLoaderState::kNotStarted, network_loader_state_);
  network_loader_state_ = NetworkLoaderState::kLoadingHeader;
}

ServiceWorkerNewScriptLoader::ServiceWorkerNewScriptLoader(
    uint32_t options,
    const network::ResourceRequest& original_request,
    network::mojom::URLLoaderClientPtr client,
    scoped_refptr<ServiceWorkerVersion> version)
    : request_url_(original_request.url),
      resource_type_(static_cast<ResourceType>(original_request.resource_type)),
      version_(std::move(version)),
      network_client_binding_(this),
      network_watcher_(FROM_HERE,
                       mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                       base::SequencedTaskRunnerHandle::Get()),
      client_(std::move(client)),
      original_options_(options),
      type_(Type::kResume),
      client_producer_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunnerHandle::Get()),
      request_start_(base::TimeTicks::Now()) {
#if DCHECK_IS_ON()
  CheckVersionStatusBeforeLoad();
#endif  // DCHECK_IS_ON()

  DCHECK(client_);
  auto paused_state = version_->TakePausedStateOfChangedScript(request_url_);

  // TODO(https://crbug.com/648295): Handle the case where it returns nullptr,
  // which means the imported script was not available when checking the update.
  DCHECK(paused_state);

  cache_writer_ = std::move(paused_state->cache_writer);
  DCHECK(cache_writer_);

  network_loader_ = std::move(paused_state->network_loader);
  DCHECK(network_loader_);

  network_client_request_ = std::move(paused_state->network_client_request);
  DCHECK(network_client_request_);

  network_consumer_ = std::move(paused_state->network_consumer);

  // Headers must have already been received during update check.
  header_writer_state_ = WriterState::kCompleted;

  network_loader_state_ = paused_state->network_loader_state;
  DCHECK(network_loader_state_ == NetworkLoaderState::kLoadingBody ||
         network_loader_state_ == NetworkLoaderState::kCompleted);

  body_writer_state_ = paused_state->body_writer_state;
  DCHECK(body_writer_state_ == WriterState::kWriting ||
         body_writer_state_ == WriterState::kCompleted);

  version_->script_cache_map()->NotifyStartedCaching(
      request_url_, cache_writer_->WriterResourceId());

  // Resume the cache writer and observe its writes, so all data written
  // is sent to |client_|.
  cache_writer_->set_write_observer(this);
  net::Error error = cache_writer_->Resume(
      base::BindOnce(&ServiceWorkerNewScriptLoader::OnCacheWriterResumed,
                     weak_factory_.GetWeakPtr()));

  if (error != net::ERR_IO_PENDING) {
    OnCacheWriterResumed(error);
  }
}

ServiceWorkerNewScriptLoader::~ServiceWorkerNewScriptLoader() = default;

void ServiceWorkerNewScriptLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  // Resource requests for service worker scripts should not follow redirects.
  // See comments in OnReceiveRedirect().
  NOTREACHED();
}

void ServiceWorkerNewScriptLoader::ProceedWithResponse() {
  NOTREACHED();
}

void ServiceWorkerNewScriptLoader::SetPriority(net::RequestPriority priority,
                                               int32_t intra_priority_value) {
  if (network_loader_)
    network_loader_->SetPriority(priority, intra_priority_value);
}

void ServiceWorkerNewScriptLoader::PauseReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->PauseReadingBodyFromNet();
}

void ServiceWorkerNewScriptLoader::ResumeReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->ResumeReadingBodyFromNet();
}

// URLLoaderClient for network loader ------------------------------------------

void ServiceWorkerNewScriptLoader::OnReceiveResponse(
    const network::ResourceResponseHead& response_head) {
  DCHECK_EQ(type_, Type::kNetworkOnly);
  DCHECK_EQ(NetworkLoaderState::kLoadingHeader, network_loader_state_);
  if (!version_->context() || version_->is_redundant()) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }

  network::URLLoaderCompletionStatus completion_status;
  std::string error_message;
  std::unique_ptr<net::HttpResponseInfo> response_info =
      service_worker_loader_helpers::CreateHttpResponseInfoAndCheckHeaders(
          response_head, &completion_status, &error_message);
  if (completion_status.error_code != net::OK) {
    CommitCompleted(completion_status, error_message);
    return;
  }

  if (resource_type_ == ResourceType::kServiceWorker) {
    // Check the path restriction defined in the spec:
    // https://w3c.github.io/ServiceWorker/#service-worker-script-response
    std::string service_worker_allowed;
    bool has_header = response_head.headers->EnumerateHeader(
        nullptr, ServiceWorkerConsts::kServiceWorkerAllowed,
        &service_worker_allowed);
    if (!ServiceWorkerUtils::IsPathRestrictionSatisfied(
            version_->scope(), request_url_,
            has_header ? &service_worker_allowed : nullptr, &error_message)) {
      CommitCompleted(
          network::URLLoaderCompletionStatus(net::ERR_INSECURE_RESPONSE),
          error_message);
      return;
    }

    if (response_head.network_accessed)
      version_->embedded_worker()->OnNetworkAccessedForScriptLoad();

    version_->SetMainScriptHttpResponseInfo(*response_info);
  }

  network_loader_state_ = NetworkLoaderState::kWaitingForBody;

  WriteHeaders(
      base::MakeRefCounted<HttpResponseInfoIOBuffer>(std::move(response_info)));

  // Don't pass SSLInfo to the client when the original request doesn't ask
  // to send it.
  if (response_head.ssl_info.has_value() &&
      !(original_options_ &
        network::mojom::kURLLoadOptionSendSSLInfoWithResponse)) {
    network::ResourceResponseHead new_response_head = response_head;
    new_response_head.ssl_info.reset();
    client_->OnReceiveResponse(new_response_head);
    return;
  }
  client_->OnReceiveResponse(response_head);
}

void ServiceWorkerNewScriptLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  DCHECK_EQ(type_, Type::kNetworkOnly);
  // Resource requests for service worker scripts should not follow redirects.
  //
  // Step 9.5: "Set request's redirect mode to "error"."
  // https://w3c.github.io/ServiceWorker/#update-algorithm
  //
  // TODO(https://crbug.com/889798): Follow redirects for imported scripts.
  CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_UNSAFE_REDIRECT),
                  ServiceWorkerConsts::kServiceWorkerRedirectError);
}

void ServiceWorkerNewScriptLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  DCHECK_EQ(type_, Type::kNetworkOnly);
  client_->OnUploadProgress(current_position, total_size,
                            std::move(ack_callback));
}

void ServiceWorkerNewScriptLoader::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {
  client_->OnReceiveCachedMetadata(std::move(data));
}

void ServiceWorkerNewScriptLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ServiceWorkerNewScriptLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle consumer) {
  DCHECK_EQ(type_, Type::kNetworkOnly);
  DCHECK_EQ(NetworkLoaderState::kWaitingForBody, network_loader_state_);
  // Create a pair of the consumer and producer for responding to the client.
  mojo::ScopedDataPipeConsumerHandle client_consumer;
  if (mojo::CreateDataPipe(nullptr, &client_producer_, &client_consumer) !=
      MOJO_RESULT_OK) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }

  // Pass the consumer handle for responding with the response to the client.
  client_->OnStartLoadingResponseBody(std::move(client_consumer));

  network_consumer_ = std::move(consumer);
  network_loader_state_ = NetworkLoaderState::kLoadingBody;
  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerNewScriptLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  NetworkLoaderState previous_state = network_loader_state_;
  network_loader_state_ = NetworkLoaderState::kCompleted;
  if (status.error_code != net::OK) {
    CommitCompleted(status,
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }

  // Response body is empty.
  if (previous_state == NetworkLoaderState::kWaitingForBody) {
    // Type::kResume doesn't reach here since OnComplete() must have been
    // called during update check when the body is empty.
    DCHECK_EQ(type_, Type::kNetworkOnly);
    DCHECK_EQ(WriterState::kNotStarted, body_writer_state_);
    body_writer_state_ = WriterState::kCompleted;
    switch (header_writer_state_) {
      case WriterState::kNotStarted:
        NOTREACHED()
            << "Response header should be received before OnComplete()";
        break;
      case WriterState::kWriting:
        // Wait until it's written. OnWriteHeadersComplete() will call
        // CommitCompleted().
        return;
      case WriterState::kCompleted:
        DCHECK(!network_consumer_.is_valid());
        CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                        std::string() /* status_message */);
        return;
    }
    NOTREACHED();
  }

  // Response body exists.
  if (previous_state == NetworkLoaderState::kLoadingBody) {
    switch (body_writer_state_) {
      case WriterState::kNotStarted:
        DCHECK_EQ(type_, Type::kNetworkOnly);
        // Wait until it's written. OnNetworkDataAvailable() will call
        // CommitCompleted() after all data from |network_consumer_| is
        // consumed.
        DCHECK_EQ(WriterState::kWriting, header_writer_state_);
        return;
      case WriterState::kWriting:
        // Wait until it's written. OnNetworkDataAvailable() will call
        // CommitCompleted() after all data from |network_consumer_| is
        // consumed.
        DCHECK_EQ(WriterState::kCompleted, header_writer_state_);
        return;
      case WriterState::kCompleted:
        DCHECK_EQ(WriterState::kCompleted, header_writer_state_);
        CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                        std::string() /* status_message */);
        return;
    }
    NOTREACHED();
  }

  NOTREACHED();
}

// End of URLLoaderClient ------------------------------------------------------

int ServiceWorkerNewScriptLoader::WillWriteInfo(
    scoped_refptr<HttpResponseInfoIOBuffer> response_info) {
  DCHECK_EQ(type_, Type::kResume);
  DCHECK(response_info);
  const net::HttpResponseInfo* info = response_info->http_info.get();
  DCHECK(info);

  if (resource_type_ == ResourceType::kServiceWorker) {
    version_->SetMainScriptHttpResponseInfo(*info);
  }

  auto response = ServiceWorkerUtils::CreateResourceResponseHeadAndMetadata(
      info, original_options_, request_start_, base::TimeTicks::Now(),
      response_info->response_data_size);
  client_->OnReceiveResponse(std::move(response.head));
  if (!response.metadata.empty())
    client_->OnReceiveCachedMetadata(std::move(response.metadata));

  mojo::ScopedDataPipeConsumerHandle client_consumer;
  if (mojo::CreateDataPipe(nullptr, &client_producer_, &client_consumer) !=
      MOJO_RESULT_OK) {
    // Reports error to cache writer and finally the loader would process this
    // failure in OnCacheWriterResumed()
    return net::ERR_INSUFFICIENT_RESOURCES;
  }

  // Pass the consumer handle to the client.
  client_->OnStartLoadingResponseBody(std::move(client_consumer));
  client_producer_watcher_.Watch(
      client_producer_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&ServiceWorkerNewScriptLoader::OnClientWritable,
                          weak_factory_.GetWeakPtr()));
  return net::OK;
}

void ServiceWorkerNewScriptLoader::OnClientWritable(MojoResult) {
  DCHECK_EQ(type_, Type::kResume);
  DCHECK(data_to_send_);
  DCHECK_GE(data_length_, bytes_sent_to_client_);
  DCHECK(client_producer_);

  // Cap the buffer size up to |kReadBufferSize|. The remaining will be written
  // next time.
  uint32_t bytes_newly_sent =
      std::min<uint32_t>(kReadBufferSize, data_length_ - bytes_sent_to_client_);

  MojoResult result =
      client_producer_->WriteData(data_to_send_->data() + bytes_sent_to_client_,
                                  &bytes_newly_sent, MOJO_WRITE_DATA_FLAG_NONE);

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // No data was written to |client_producer_| because the pipe was full.
    // Retry when the pipe becomes ready again.
    client_producer_watcher_.ArmOrNotify();
    return;
  }

  if (result != MOJO_RESULT_OK) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_DATA_ERROR);
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }

  bytes_sent_to_client_ += bytes_newly_sent;
  if (bytes_sent_to_client_ != data_length_) {
    // Not all data is sent. Send the rest in another task.
    client_producer_watcher_.ArmOrNotify();
    return;
  }
  std::move(write_observer_complete_callback_).Run(net::OK);
}

int ServiceWorkerNewScriptLoader::WillWriteData(
    scoped_refptr<net::IOBuffer> data,
    int length,
    base::OnceCallback<void(net::Error)> callback) {
  DCHECK_EQ(type_, Type::kResume);
  DCHECK(!write_observer_complete_callback_);
  DCHECK(client_producer_);

  data_to_send_ = std::move(data);
  data_length_ = length;
  bytes_sent_to_client_ = 0;
  write_observer_complete_callback_ = std::move(callback);
  client_producer_watcher_.ArmOrNotify();
  return net::ERR_IO_PENDING;
}

void ServiceWorkerNewScriptLoader::OnCacheWriterResumed(net::Error error) {
  DCHECK_EQ(type_, Type::kResume);
  DCHECK_NE(error, net::ERR_IO_PENDING);
  // Stop observing write operations in cache writer as further data are
  // from network which would be processed by OnNetworkDataAvailable().
  cache_writer_->set_write_observer(nullptr);

  if (error != net::OK) {
    CommitCompleted(network::URLLoaderCompletionStatus(error),
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }
  // If the script has no body or all the body has already been read when it
  // was paused, we don't have to wait for more data from network.
  if (body_writer_state_ == WriterState::kCompleted) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::OK), std::string());
    return;
  }

  // Continue to load the rest of the body from the network.
  DCHECK_EQ(body_writer_state_, WriterState::kWriting);
  DCHECK(network_consumer_);
  network_client_binding_.Bind(std::move(network_client_request_));
  network_watcher_.Watch(
      network_consumer_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&ServiceWorkerNewScriptLoader::OnNetworkDataAvailable,
                          weak_factory_.GetWeakPtr()));
  network_watcher_.ArmOrNotify();
}

#if DCHECK_IS_ON()
void ServiceWorkerNewScriptLoader::CheckVersionStatusBeforeLoad() {
  DCHECK(version_);

  // ServiceWorkerNewScriptLoader is used for fetching the service worker main
  // script (RESOURCE_TYPE_SERVICE_WORKER) during worker startup or
  // importScripts() (RESOURCE_TYPE_SCRIPT).
  // TODO(nhiroki): In the current implementation, importScripts() can be called
  // in any ServiceWorkerVersion::Status except for REDUNDANT, but the spec
  // defines importScripts() works only on the initial script evaluation and the
  // install event. Update this check once importScripts() is fixed.
  // (https://crbug.com/719052)
  DCHECK((resource_type_ == ResourceType::kServiceWorker &&
          version_->status() == ServiceWorkerVersion::NEW) ||
         (resource_type_ == ResourceType::kScript &&
          version_->status() != ServiceWorkerVersion::REDUNDANT));
}
#endif  // DCHECK_IS_ON()

void ServiceWorkerNewScriptLoader::WriteHeaders(
    scoped_refptr<HttpResponseInfoIOBuffer> info_buffer) {
  DCHECK_EQ(type_, Type::kNetworkOnly);
  DCHECK_EQ(WriterState::kNotStarted, header_writer_state_);
  header_writer_state_ = WriterState::kWriting;
  net::Error error = cache_writer_->MaybeWriteHeaders(
      info_buffer.get(),
      base::BindOnce(&ServiceWorkerNewScriptLoader::OnWriteHeadersComplete,
                     weak_factory_.GetWeakPtr()));
  if (error == net::ERR_IO_PENDING) {
    // OnWriteHeadersComplete() will be called asynchronously.
    return;
  }
  // MaybeWriteHeaders() doesn't run the callback if it finishes synchronously,
  // so explicitly call it here.
  OnWriteHeadersComplete(error);
}

void ServiceWorkerNewScriptLoader::OnWriteHeadersComplete(net::Error error) {
  DCHECK_EQ(WriterState::kWriting, header_writer_state_);
  DCHECK_NE(net::ERR_IO_PENDING, error);
  if (error != net::OK) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_HEADERS_ERROR);
    CommitCompleted(network::URLLoaderCompletionStatus(error),
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }
  header_writer_state_ = WriterState::kCompleted;

  // If all other states are kCompleted the response body is empty, we can
  // finish now.
  if (network_loader_state_ == NetworkLoaderState::kCompleted &&
      body_writer_state_ == WriterState::kCompleted) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                    std::string() /* status_message */);
    return;
  }

  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerNewScriptLoader::MaybeStartNetworkConsumerHandleWatcher() {
  if (network_loader_state_ == NetworkLoaderState::kWaitingForBody) {
    // OnStartLoadingResponseBody() or OnComplete() will continue the sequence.
    return;
  }
  if (header_writer_state_ != WriterState::kCompleted) {
    DCHECK_EQ(WriterState::kWriting, header_writer_state_);
    // OnWriteHeadersComplete() will continue the sequence.
    return;
  }

  DCHECK_EQ(WriterState::kNotStarted, body_writer_state_);
  body_writer_state_ = WriterState::kWriting;

  network_watcher_.Watch(
      network_consumer_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&ServiceWorkerNewScriptLoader::OnNetworkDataAvailable,
                          weak_factory_.GetWeakPtr()));
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerNewScriptLoader::OnNetworkDataAvailable(MojoResult) {
  DCHECK_EQ(WriterState::kCompleted, header_writer_state_);
  DCHECK_EQ(WriterState::kWriting, body_writer_state_);
  DCHECK(network_consumer_.is_valid());
  scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer;
  uint32_t bytes_available = 0;
  MojoResult result = network::MojoToNetPendingBuffer::BeginRead(
      &network_consumer_, &pending_buffer, &bytes_available);
  switch (result) {
    case MOJO_RESULT_OK:
      WriteData(std::move(pending_buffer), bytes_available);
      return;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // Call WriteData() with null buffer to let the cache writer know that
      // body from the network reaches to the end.
      WriteData(/*pending_buffer=*/nullptr, /*bytes_available=*/0);
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      network_watcher_.ArmOrNotify();
      return;
  }
  NOTREACHED() << static_cast<int>(result);
}

void ServiceWorkerNewScriptLoader::WriteData(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    uint32_t bytes_available) {
  // Cap the buffer size up to |kReadBufferSize|. The remaining will be written
  // next time.
  uint32_t bytes_written = std::min<uint32_t>(kReadBufferSize, bytes_available);

  auto buffer = base::MakeRefCounted<WrappedIOBuffer>(
      pending_buffer ? pending_buffer->buffer() : nullptr);
  MojoResult result = client_producer_->WriteData(
      buffer->data(), &bytes_written, MOJO_WRITE_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      ServiceWorkerMetrics::CountWriteResponseResult(
          ServiceWorkerMetrics::WRITE_DATA_ERROR);
      CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                      ServiceWorkerConsts::kServiceWorkerFetchScriptError);
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      // No data was written to |client_producer_| because the pipe was full.
      // Retry when the pipe becomes ready again.
      pending_buffer->CompleteRead(0);
      network_consumer_ = pending_buffer->ReleaseHandle();
      network_watcher_.ArmOrNotify();
      return;
    default:
      NOTREACHED() << static_cast<int>(result);
      return;
  }

  // Write the buffer in the service worker script storage up to the size we
  // successfully wrote to the data pipe (i.e., |bytes_written|).
  // A null buffer and zero |bytes_written| are passed when this is the end of
  // the body.
  net::Error error = cache_writer_->MaybeWriteData(
      buffer.get(), base::strict_cast<size_t>(bytes_written),
      base::BindOnce(&ServiceWorkerNewScriptLoader::OnWriteDataComplete,
                     weak_factory_.GetWeakPtr(), pending_buffer,
                     bytes_written));
  if (error == net::ERR_IO_PENDING) {
    // OnWriteDataComplete() will be called asynchronously.
    return;
  }
  // MaybeWriteData() doesn't run the callback if it finishes synchronously, so
  // explicitly call it here.
  OnWriteDataComplete(std::move(pending_buffer), bytes_written, error);
}

void ServiceWorkerNewScriptLoader::OnWriteDataComplete(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    uint32_t bytes_written,
    net::Error error) {
  DCHECK_NE(net::ERR_IO_PENDING, error);
  if (error != net::OK) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_DATA_ERROR);
    CommitCompleted(network::URLLoaderCompletionStatus(error),
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }
  ServiceWorkerMetrics::CountWriteResponseResult(
      ServiceWorkerMetrics::WRITE_OK);

  if (bytes_written == 0) {
    // Zero |bytes_written| with net::OK means that all data has been read from
    // the network and the Mojo data pipe has been closed. Thus we can complete
    // the request if OnComplete() has already been received.
    DCHECK(!pending_buffer);
    body_writer_state_ = WriterState::kCompleted;
    if (network_loader_state_ == NetworkLoaderState::kCompleted) {
      CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                      std::string() /* status_message */);
    }
    return;
  }

  DCHECK(pending_buffer);
  pending_buffer->CompleteRead(bytes_written);
  // Get the consumer handle from a previous read operation if we have one.
  network_consumer_ = pending_buffer->ReleaseHandle();
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerNewScriptLoader::CommitCompleted(
    const network::URLLoaderCompletionStatus& status,
    const std::string& status_message) {
  net::Error error_code = static_cast<net::Error>(status.error_code);
  int bytes_written = -1;
  if (error_code == net::OK) {
    DCHECK_EQ(NetworkLoaderState::kCompleted, network_loader_state_);
    DCHECK_EQ(WriterState::kCompleted, header_writer_state_);
    DCHECK_EQ(WriterState::kCompleted, body_writer_state_);
    // If all the calls to WriteHeaders/WriteData succeeded, but the incumbent
    // entry wasn't actually replaced because the new entry was equivalent, the
    // new version didn't actually install because it already exists.
    if (!cache_writer_->did_replace()) {
      version_->SetStartWorkerStatusCode(
          blink::ServiceWorkerStatusCode::kErrorExists);
      error_code = net::ERR_FILE_EXISTS;
    }
    bytes_written = cache_writer_->bytes_written();
  } else {
    // AddMessageConsole must be called before notifying that an error occurred
    // because the worker stops soon after receiving the error response.
    // TODO(nhiroki): Consider replacing this hacky way with the new error code
    // handling mechanism in URLLoader.
    version_->embedded_worker()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError, status_message);
  }
  version_->script_cache_map()->NotifyFinishedCaching(
      request_url_, bytes_written, error_code, status_message);

  client_->OnComplete(status);
  client_producer_.reset();
  client_producer_watcher_.Cancel();

  network_loader_.reset();
  network_client_binding_.Close();
  network_consumer_.reset();
  network_watcher_.Cancel();
  cache_writer_.reset();
  network_loader_state_ = NetworkLoaderState::kCompleted;
  header_writer_state_ = WriterState::kCompleted;
  body_writer_state_ = WriterState::kCompleted;
}

}  // namespace content
