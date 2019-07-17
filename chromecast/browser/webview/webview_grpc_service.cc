// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_grpc_service.h"

#include <deque>
#include <mutex>

#include "base/bind.h"
#include "base/callback.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/webview/webview_controller.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"
#include "third_party/grpc/src/include/grpcpp/security/server_credentials.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"

namespace chromecast {
namespace {

aura::Window* FindChildWindowWithID(aura::Window* window, int id) {
  if (window->GetProperty(exo::kClientSurfaceIdKey) == id)
    return window;
  for (auto* w : window->children()) {
    auto* ret = FindChildWindowWithID(w, id);
    if (ret)
      return ret;
  }
  return nullptr;
}

typedef base::RepeatingCallback<void(bool)> GrpcCallback;

// Threading model and life cycle.
// WebviewRpcInstance creates copies of itself and deletes itself as needed.
// Instances are deleted when the GRPC Finish request completes and there are
// no other outstanding read or write operations.
// Deletion bounces off the webview thread to synchronize with request
// processing.
class WebviewRpcInstance : public WebviewController::Client {
 public:
  WebviewRpcInstance(webview::WebviewService::AsyncService* service,
                     grpc::ServerCompletionQueue* cq,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebviewRpcInstance() override;

 private:
  void InitComplete(bool ok);
  void ReadComplete(bool ok);
  void WriteComplete(bool ok);
  void FinishComplete(bool ok);
  bool Initialize();

  void StartRead();
  void CreateWebview(int app_id, int window_id);

  void ProcessRequestOnWebviewThread(
      std::unique_ptr<webview::WebviewRequest> request);

  void EnqueueSend(std::unique_ptr<webview::WebviewResponse> response) override;
  void OnError(const std::string& error_message) override;

  GrpcCallback init_callback_;
  GrpcCallback read_callback_;
  GrpcCallback write_callback_;
  GrpcCallback destroy_callback_;

  grpc::ServerContext ctx_;
  webview::WebviewService::AsyncService* service_;
  grpc::ServerCompletionQueue* cq_;
  std::unique_ptr<webview::WebviewRequest> request_;
  grpc::ServerAsyncReaderWriter<webview::WebviewResponse,
                                webview::WebviewRequest>
      io_;
  std::unique_ptr<WebviewController> webview_;

  std::mutex send_lock_;
  bool errored_ = false;
  std::string error_message_;
  bool send_pending_ = false;
  bool destroying_ = false;
  std::deque<std::unique_ptr<webview::WebviewResponse>> pending_messages_;

  grpc::WriteOptions write_options_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebviewRpcInstance);
};

WebviewRpcInstance::WebviewRpcInstance(
    webview::WebviewService::AsyncService* service,
    grpc::ServerCompletionQueue* cq,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : service_(service), cq_(cq), io_(&ctx_), task_runner_(task_runner) {
  write_options_.clear_buffer_hint();
  write_options_.clear_corked();

  init_callback_ = base::BindRepeating(&WebviewRpcInstance::InitComplete,
                                       base::Unretained(this));
  read_callback_ = base::BindRepeating(&WebviewRpcInstance::ReadComplete,
                                       base::Unretained(this));
  write_callback_ = base::BindRepeating(&WebviewRpcInstance::WriteComplete,
                                        base::Unretained(this));
  destroy_callback_ = base::BindRepeating(&WebviewRpcInstance::FinishComplete,
                                          base::Unretained(this));

  service_->RequestCreateWebview(&ctx_, &io_, cq_, cq_, &init_callback_);
}

WebviewRpcInstance::~WebviewRpcInstance() {
  DCHECK(destroying_);
  if (webview_) {
    webview_.release()->Destroy();
  }
}

void WebviewRpcInstance::FinishComplete(bool ok) {
  // Bounce off of the webview thread to allow it to complete any pending work.
  destroying_ = true;
  if (!send_pending_) {
    task_runner_->DeleteSoon(FROM_HERE, this);
  }
}

void WebviewRpcInstance::ProcessRequestOnWebviewThread(
    std::unique_ptr<webview::WebviewRequest> request) {
  webview_->ProcessRequest(*request.get());
}

void WebviewRpcInstance::InitComplete(bool ok) {
  request_ = std::make_unique<webview::WebviewRequest>();
  io_.Read(request_.get(), &read_callback_);

  // Create a new instance to handle new requests.
  // Instances of this class delete themselves.
  new WebviewRpcInstance(service_, cq_, task_runner_);
}

void WebviewRpcInstance::ReadComplete(bool ok) {
  if (!ok) {
    io_.Finish(grpc::Status(), &destroy_callback_);
  } else if (webview_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebviewRpcInstance::ProcessRequestOnWebviewThread,
                       base::Unretained(this),
                       base::Passed(std::move(request_))));

    request_ = std::make_unique<webview::WebviewRequest>();
    io_.Read(request_.get(), &read_callback_);
  } else if (!Initialize()) {
    io_.Finish(grpc::Status(grpc::FAILED_PRECONDITION, "Failed initialization"),
               &destroy_callback_);
  }
  // In this case initialization is pending and the main webview thread will
  // start the first read.
}

bool WebviewRpcInstance::Initialize() {
  if (request_->type_case() != webview::WebviewRequest::kCreate)
    return false;

  // This needs to be done on a valid thread.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebviewRpcInstance::CreateWebview, base::Unretained(this),
                     request_->create().webview_id(),
                     request_->create().window_id()));
  return true;
}

void WebviewRpcInstance::CreateWebview(int app_id, int window_id) {
  auto* root_window =
      exo::WMHelper::GetInstance()->GetRootWindowForNewWindows();
  auto* surface_window = FindChildWindowWithID(root_window, app_id);
  if (!surface_window) {
    OnError("Failed to find valid surface to display webview on");
    return;
  }

  content::BrowserContext* browser_context =
      shell::CastBrowserProcess::GetInstance()->browser_context();
  webview_ = std::make_unique<WebviewController>(browser_context, this);
  webview_->AttachTo(surface_window, window_id);

  // Begin reading again.
  io_.Read(request_.get(), &read_callback_);
}

void WebviewRpcInstance::WriteComplete(bool ok) {
  std::unique_lock<std::mutex> l(send_lock_);
  send_pending_ = false;
  if (destroying_) {
    // It is possible for the read & finish to complete while a write is
    // outstanding, in that case just re-call it to delete this instance.
    FinishComplete(true);
  } else if (errored_) {
    io_.Finish(grpc::Status(grpc::UNKNOWN, error_message_), &destroy_callback_);
  } else if (!pending_messages_.empty()) {
    send_pending_ = true;
    io_.Write(*pending_messages_.front().get(), write_options_,
              &write_callback_);
    pending_messages_.pop_front();
  }
}

void WebviewRpcInstance::EnqueueSend(
    std::unique_ptr<webview::WebviewResponse> response) {
  std::unique_lock<std::mutex> l(send_lock_);
  if (errored_ || destroying_)
    return;
  if (!send_pending_ && pending_messages_.empty()) {
    send_pending_ = true;
    io_.Write(*response.get(), write_options_, &write_callback_);
  } else {
    pending_messages_.emplace_back(std::move(response));
  }
}

void WebviewRpcInstance::OnError(const std::string& error_message) {
  std::unique_lock<std::mutex> l(send_lock_);
  errored_ = true;
  error_message_ = error_message;

  if (!send_pending_)
    io_.Finish(grpc::Status(grpc::UNKNOWN, error_message_), &destroy_callback_);
  send_pending_ = true;
}

}  // namespace

WebviewAsyncService::WebviewAsyncService(
    scoped_refptr<base::SingleThreadTaskRunner> webview_task_runner)
    : webview_task_runner_(std::move(webview_task_runner)) {}

WebviewAsyncService::~WebviewAsyncService() {
  if (server_) {
    server_->Shutdown();
    cq_->Shutdown();

    base::PlatformThread::Join(rpc_thread_);
  }
}

void WebviewAsyncService::StartWithSocket(const base::FilePath& socket_path) {
  DCHECK(!server_.get());

  grpc::ServerBuilder builder;
  builder.AddListeningPort("unix:" + socket_path.value(),
                           grpc::InsecureServerCredentials());
  builder.RegisterService(&service_);
  cq_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();
  base::PlatformThread::Create(0, this, &rpc_thread_);
}

void WebviewAsyncService::ThreadMain() {
  base::PlatformThread::SetName("CastWebviewGrpcMessagePump");

  void* tag;
  bool ok;
  // This self-deletes.
  new WebviewRpcInstance(&service_, cq_.get(), webview_task_runner_);
  // This thread is joined when this service is destroyed.
  while (cq_->Next(&tag, &ok)) {
    reinterpret_cast<GrpcCallback*>(tag)->Run(ok);
  }
}

}  // namespace chromecast
