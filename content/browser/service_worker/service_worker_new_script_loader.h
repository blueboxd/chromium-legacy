// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NEW_SCRIPT_LOADER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NEW_SCRIPT_LOADER_H_

#include "base/macros.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerVersion;
struct HttpResponseInfoIOBuffer;

// This is the URLLoader used for loading scripts for a new (installing) service
// worker. It fetches the script (the main script or imported script) and
// returns the response to |client|, while also writing the response into the
// service worker script storage.
//
// There are two types of ServiceWorkerNewScriptLoader:
//   1. Network-only type
//      This is for downloading a new script from network only.
//   2. Resume type
//      This loader reads part of the script, which has already been loaded
//      during an update check, from ServiceWorkerCacheWriter and resumes to
//      load the rest of the script from the network after that. See also
//      ServiceWorkerUpdateChecker for more details.
//
// In the common case, the network-only type loader works as follows:
//   1. Makes a network request.
//   2. OnReceiveResponse() is called, writes the response headers to the
//      service worker script storage and responds with them to the |client|
//      (which is the service worker in the renderer).
//   3. OnStartLoadingResponseBody() is called, reads the network response from
//      the data pipe. While reading the response, writes it to the service
//      worker script storage and responds with it to the |client|.
//   4. OnComplete() for the network load and OnWriteDataComplete() are called,
//      calls CommitCompleted() and closes the connections with the network
//      service and the renderer process.
// In an uncommon case, the response body is empty so
// OnStartLoadingResponseBody() is not called.
//
// The work flow of resume type loader is different from network-only type:
// OnReceiveResponse() and OnStartLoadingResponseBody() are not called as they
// must have been called during update check.
//
// In the common case, the resume type loader works as follows:
//   1. The ServiceWorkerCacheWriter used in the update check is resumed. This
//      will read part of the script data already loaded and respond with it
//      to the |client| and write to service worker script storage.
//   2. The network download is resumed. The rest of the script data is
//      responded to the |client| and written to service worker storage.
//   3. OnComplete() for the network load and OnWriteDataComplete() are called,
//      calls CommitCompleted() and closes the connections with the network
//      service and the renderer process.
// In an uncommon case, OnComplete() is not called. For example, if a script
// was changed to have no body. OnComplete() would have been called during
// update check to find this change, thus it would never be called again.
// In this case, CommitCompleted() would be called after
// ServiceWorkerCacheWriter::Resume() is done.
//
// A set of |network_loader_state_|, |header_writer_state_|, and
// |body_writer_state_| is the state of this loader. Each of them is changed
// independently, while some state changes have dependency to other state
// changes.  See the comment for each field below to see exactly when their
// state changes happen. For resume loaders, these states are set to be
// values extracted from ServiceWorkerSingleScriptUpdateChecker::PausedState
// to make the loader seamlessly resume the download.
//
// In case there is already an installed service worker for this registration,
// this class also performs the "byte-for-byte" comparison for updating the
// worker. If the script is identical, the load succeeds but no script is
// written, and ServiceWorkerVersion is told to terminate startup.
//
// NOTE: To perform the network request, this class uses |loader_factory_| which
// may internally use a non-NetworkService factory if URL has a non-http(s)
// scheme, e.g., a chrome-extension:// URL. Regardless, that is still called a
// "network" request in comments and naming. "network" is meant to distinguish
// from the load this URLLoader does for its client:
//     "network" <------> SWNewScriptLoader <------> client
class CONTENT_EXPORT ServiceWorkerNewScriptLoader
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient,
      public ServiceWorkerCacheWriter::WriteObserver {
 public:
  enum class Type {
    kNetworkOnly,  // For loaders created by CreateForNetworkOnly().
    kResume,       // For loaders created by CreateForResume().
  };

  enum class NetworkLoaderState {
    kNotStarted,
    kLoadingHeader,
    kWaitingForBody,
    kLoadingBody,
    kCompleted,
  };

  enum class WriterState { kNotStarted, kWriting, kCompleted };

  // Creates a loader for a script to be loaded entirely from the network.
  static std::unique_ptr<ServiceWorkerNewScriptLoader> CreateForNetworkOnly(
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& original_request,
      network::mojom::URLLoaderClientPtr client,
      scoped_refptr<ServiceWorkerVersion> version,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  // ServiceWorkerImportedScriptUpdateCheck:
  // Creates a loader to continue downloading of a script paused during update
  // check.
  static std::unique_ptr<ServiceWorkerNewScriptLoader> CreateForResume(
      uint32_t options,
      const network::ResourceRequest& original_request,
      network::mojom::URLLoaderClientPtr client,
      scoped_refptr<ServiceWorkerVersion> version);

  ~ServiceWorkerNewScriptLoader() override;

  // network::mojom::URLLoader:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient for the network load:
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // Implements ServiceWorkerCacheWriter::WriteObserver.
  // These two methods are only used for resume loaders.
  int WillWriteInfo(
      scoped_refptr<HttpResponseInfoIOBuffer> response_info) override;
  int WillWriteData(scoped_refptr<net::IOBuffer> data,
                    int length,
                    base::OnceCallback<void(net::Error)> callback) override;

  Type type() const { return type_; }

  // Buffer size for reading script data from network.
  const static uint32_t kReadBufferSize;

 private:
  class WrappedIOBuffer;

  // This is for constructing network-only script loaders.
  // |loader_factory| is used to load the script, see class comments.
  ServiceWorkerNewScriptLoader(
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& original_request,
      network::mojom::URLLoaderClientPtr client,
      scoped_refptr<ServiceWorkerVersion> version,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  // This is for constructing resume loaders.
  ServiceWorkerNewScriptLoader(uint32_t options,
                               const network::ResourceRequest& original_request,
                               network::mojom::URLLoaderClientPtr client,
                               scoped_refptr<ServiceWorkerVersion> version);

  // Writes the given headers into the service worker script storage.
  void WriteHeaders(scoped_refptr<HttpResponseInfoIOBuffer> info_buffer);
  void OnWriteHeadersComplete(net::Error error);

  // Starts watching the data pipe for the network load (i.e.,
  // |network_consumer_|) if it's ready.
  void MaybeStartNetworkConsumerHandleWatcher();

  // Called when |network_consumer_| is ready to be read. Can be called multiple
  // times.
  void OnNetworkDataAvailable(MojoResult);

  // Writes the given data into the service worker script storage.
  void WriteData(scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
                 uint32_t bytes_available);
  void OnWriteDataComplete(
      scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
      uint32_t bytes_written,
      net::Error error);

  // This is the last method that is called on this class. Notifies the final
  // result to |client_| and clears all mojo connections etc.
  void CommitCompleted(const network::URLLoaderCompletionStatus& status,
                       const std::string& status_message);

  // Called when |client_producer_| is writable. It writes |data_to_send_|
  // to |client_producer_|. If all data is written, the observer has completed
  // its work and |write_observer_complete_callback_| is called. Otherwise,
  // |client_producer_watcher_| is armed to wait for |client_producer_| to be
  // writable again.
  void OnClientWritable(MojoResult);

  // Called when ServiceWorkerCacheWriter::Resume() completes its work.
  // If not all data are received, it continues to download from network.
  void OnCacheWriterResumed(net::Error error);

#if DCHECK_IS_ON()
  void CheckVersionStatusBeforeLoad();
#endif  // DCHECK_IS_ON()

  const GURL request_url_;

  // This is ResourceType::kServiceWorker for the main script or
  // ResourceType::kScript for an imported script.
  const ResourceType resource_type_;

  scoped_refptr<ServiceWorkerVersion> version_;

  std::unique_ptr<ServiceWorkerCacheWriter> cache_writer_;

  // Used for fetching the script from network, which might not actually
  // use the direct network factory, see class comments.
  network::mojom::URLLoaderPtr network_loader_;
  mojo::Binding<network::mojom::URLLoaderClient> network_client_binding_;
  mojo::ScopedDataPipeConsumerHandle network_consumer_;
  mojo::SimpleWatcher network_watcher_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  // Used for responding with the fetched script to this loader's client.
  network::mojom::URLLoaderClientPtr client_;
  mojo::ScopedDataPipeProducerHandle client_producer_;

  // Represents the state of |network_loader_|.
  // Corresponds to the steps described in the class comments.
  //
  // When response body exists:
  // CreateLoaderAndStart(): kNotStarted -> kLoadingHeader
  // OnReceiveResponse(): kLoadingHeader -> kWaitingForBody
  // OnStartLoadingResponseBody(): kWaitingForBody -> kLoadingBody
  // OnComplete(): kLoadingBody -> kCompleted
  //
  // When response body is empty:
  // CreateLoaderAndStart(): kNotStarted -> kLoadingHeader
  // OnReceiveResponse(): kLoadingHeader -> kWaitingForBody
  // OnComplete(): kWaitingForBody -> kCompleted
  NetworkLoaderState network_loader_state_ = NetworkLoaderState::kNotStarted;

  // Represents the state of |cache_writer_|.
  // Set to kWriting when it starts to write the header, and set to kCompleted
  // when the header has been written.
  //
  // OnReceiveResponse(): kNotStarted -> kWriting (in WriteHeaders())
  // OnWriteHeadersComplete(): kWriting -> kCompleted
  WriterState header_writer_state_ = WriterState::kNotStarted;

  // Represents the state of |cache_writer_| and |network_consumer_|.
  // Set to kWriting when |this| starts watching |network_consumer_|, and set to
  // kCompleted when all data has been written to |cache_writer_|.
  //
  // When response body exists:
  // OnStartLoadingResponseBody() && OnWriteHeadersComplete():
  //     kNotStarted -> kWriting
  // OnNetworkDataAvailable() && MOJO_RESULT_FAILED_PRECONDITION:
  //     kWriting -> kCompleted
  //
  // When response body is empty:
  // OnComplete(): kNotStarted -> kCompleted
  WriterState body_writer_state_ = WriterState::kNotStarted;

  const uint32_t original_options_;

  const Type type_;

  // ---------- Start of Type::kResume loader members ----------
  mojo::SimpleWatcher client_producer_watcher_;
  base::TimeTicks request_start_;
  network::mojom::URLLoaderClientRequest network_client_request_;

  // This is the data notified by OnBeforeWriteData() which would be sent
  // to |client_|.
  scoped_refptr<net::IOBuffer> data_to_send_;

  // Length of |data_to_send_| in bytes.
  int data_length_ = 0;

  // Length of data in |data_to_send_| already sent to |client_|.
  int bytes_sent_to_client_ = 0;

  // Run this to notify ServiceWorkerCacheWriter that the observer completed
  // its work. net::OK means all |data_to_send_| has been sent to |client_|.
  base::OnceCallback<void(net::Error)> write_observer_complete_callback_;
  // ---------- End of Type::kResume loader members ----------

  base::WeakPtrFactory<ServiceWorkerNewScriptLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNewScriptLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NEW_SCRIPT_LOADER_H_
