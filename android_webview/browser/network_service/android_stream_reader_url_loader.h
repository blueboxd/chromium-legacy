// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_ANDROID_STREAM_READER_URL_LOADER_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_ANDROID_STREAM_READER_URL_LOADER_H_

#include "android_webview/browser/net/aw_web_resource_response.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/http/http_byte_range.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace android_webview {

class InputStream;
class InputStreamReaderWrapper;

// Custom URLLoader implementation for loading network responses from stream.
// Current implementation is in particular for supporting shouldInterceptRequest
// callback in webview.
class AndroidStreamReaderURLLoader : public network::mojom::URLLoader {
 public:
  // Delegate abstraction for obtaining input streams.
  class ResponseDelegate {
   public:
    virtual ~ResponseDelegate() {}

    // This method is called from a worker thread, not from the IO thread.
    virtual std::unique_ptr<android_webview::InputStream> OpenInputStream(
        JNIEnv* env) = 0;

    // This method is called on the URLLoader thread (IO thread) if the
    // result of calling OpenInputStream was null.
    // Returns true if the request was restarted with a new loader or
    // was completed, false otherwise.
    virtual bool OnInputStreamOpenFailed() = 0;

    virtual bool GetMimeType(JNIEnv* env,
                             const GURL& url,
                             android_webview::InputStream* stream,
                             std::string* mime_type) = 0;

    virtual bool GetCharset(JNIEnv* env,
                            const GURL& url,
                            android_webview::InputStream* stream,
                            std::string* charset) = 0;

    virtual void AppendResponseHeaders(JNIEnv* env,
                                       net::HttpResponseHeaders* headers) = 0;
  };

  AndroidStreamReaderURLLoader(
      const network::ResourceRequest& resource_request,
      network::mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      std::unique_ptr<ResponseDelegate> response_delegate);
  ~AndroidStreamReaderURLLoader() override;

  void Start();

  // network::mojom::URLLoader overrides:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

 private:
  bool ParseRange(const net::HttpRequestHeaders& headers);
  void OnInputStreamOpened(
      std::unique_ptr<AndroidStreamReaderURLLoader::ResponseDelegate>
          returned_delegate,
      std::unique_ptr<android_webview::InputStream> input_stream);
  void OnReaderSeekCompleted(int result);
  void HeadersComplete(int status_code, const std::string& status_text);
  void RequestComplete(int status_code);
  void SendBody();

  void OnDataPipeWritable(MojoResult result);
  void CleanUp();
  void DidRead(int result);
  void ReadMore();

  // Expected content size
  int64_t expected_content_size_ = -1;

  net::HttpByteRange byte_range_;
  network::ResourceRequest resource_request_;
  network::mojom::URLLoaderClientPtr client_;
  const net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  std::unique_ptr<ResponseDelegate> response_delegate_;
  scoped_refptr<InputStreamReaderWrapper> input_stream_reader_wrapper_;

  mojo::ScopedDataPipeProducerHandle producer_handle_;
  scoped_refptr<network::NetToMojoPendingBuffer> pending_buffer_;
  mojo::SimpleWatcher writable_handle_watcher_;
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<AndroidStreamReaderURLLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidStreamReaderURLLoader);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_ANDROID_STREAM_READER_URL_LOADER_H_
