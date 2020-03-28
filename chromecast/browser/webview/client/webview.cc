// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/client/webview.h"

#include <grpcpp/create_channel.h>

#include "base/bind.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/gl_bindings.h"

namespace chromecast {
namespace client {

namespace {

constexpr int kGrpcMaxReconnectBackoffMs = 1000;
constexpr int kWebviewId = 10;

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  WebviewClient* webview_client = static_cast<WebviewClient*>(data);
  if (webview_client->HasAvailableBuffer())
    webview_client->SchedulePaint();
}

void BufferReleaseCallback(void* data, wl_buffer* /* buffer */) {
  WebviewClient::BufferCallback* buffer_callback =
      static_cast<WebviewClient::BufferCallback*>(data);
  buffer_callback->buffer->busy = false;
  buffer_callback->client->SchedulePaint();
}

}  // namespace

using chromecast::webview::WebviewRequest;
using chromecast::webview::WebviewResponse;

WebviewClient::WebviewClient()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      file_descriptor_watcher_(task_runner_) {}

WebviewClient::~WebviewClient() {}

bool WebviewClient::HasAvailableBuffer() {
  auto buffer_it =
      std::find_if(buffers_.begin(), buffers_.end(),
                   [](const std::unique_ptr<ClientBase::Buffer>& buffer) {
                     return !buffer->busy;
                   });
  return buffer_it != buffers_.end();
}

void WebviewClient::Run(const InitParams& params,
                        const std::string& channel_directory) {
  webview_surface_.reset(static_cast<wl_surface*>(
      wl_compositor_create_surface(globals_.compositor.get())));

  // Roundtrip to wait for display configuration.
  wl_display_roundtrip(display_.get());

  AllocateBuffers(params);

  ::grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, kGrpcMaxReconnectBackoffMs);
  stub_ = chromecast::webview::PlatformViewsService::NewStub(
      ::grpc::CreateCustomChannel(std::string("unix:" + channel_directory),
                                  ::grpc::InsecureChannelCredentials(), args));
  std::unique_ptr<::grpc::ClientContext> context =
      std::make_unique<::grpc::ClientContext>();
  client_ = stub_->CreateWebview(context.get());
  WebviewRequest request;

  request.mutable_create()->set_webview_id(kWebviewId);
  request.mutable_create()->set_window_id(kWebviewId);

  if (!client_->Write(request)) {
    LOG(ERROR) << ("Failed to create webview");
    return;
  }

  WebviewResponse response;
  if (!client_->Read(&response)) {
    LOG(ERROR) << "Failed to read webview creation response";
    return;
  }

  wl_webview_surface_.reset(wl_subcompositor_get_subsurface(
      globals_.subcompositor.get(), webview_surface_.get(), surface_.get()));
  wl_subsurface_set_sync(wl_webview_surface_.get());
  aura_surface.reset(zaura_shell_get_aura_surface(globals_.aura_shell.get(),
                                                  webview_surface_.get()));

  if (!aura_surface) {
    LOG(ERROR) << "No aura surface";
    return;
  }

  zaura_surface_set_client_surface_id(aura_surface.get(), kWebviewId);

  WebviewRequest resize_request;
  resize_request.mutable_resize()->set_width(size_.height());
  resize_request.mutable_resize()->set_height(size_.width());

  if (!client_->Write(resize_request)) {
    LOG(ERROR) << ("Resize request failed");
    return;
  }

  std::cout << "Enter URL, q to quit: ";
  std::cout.flush();
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebviewClient::Paint, base::Unretained(this)));

  stdin_controller_ = file_descriptor_watcher_.WatchReadable(
      STDIN_FILENO, base::BindRepeating(&WebviewClient::InputCallback,
                                        base::Unretained(this)));
  TakeExclusiveAccess();
  wl_display_controller_ = file_descriptor_watcher_.WatchReadable(
      wl_display_get_fd(display_.get()),
      base::BindRepeating(&WebviewClient::WlDisplayCallback,
                          base::Unretained(this)));
  run_loop_.Run();
}

void WebviewClient::AllocateBuffers(const InitParams& params) {
  static wl_buffer_listener buffer_listener = {BufferReleaseCallback};
  for (size_t i = 0; i < params.num_buffers; ++i) {
    auto buffer_callback = std::make_unique<BufferCallback>();
    auto buffer = CreateBuffer(size_, params.drm_format, params.bo_usage,
                               &buffer_listener, buffer_callback.get());
    if (!buffer) {
      LOG(ERROR) << "Failed to create buffer";
      return;
    }
    buffer_callback->client = this;
    buffer_callback->buffer = buffer.get();
    buffer_callbacks_.push_back(std::move(buffer_callback));
    buffers_.push_back(std::move(buffer));
  }
  webview_buffer_ = CreateBuffer(size_, params.drm_format, params.bo_usage);
}

void WebviewClient::HandleMode(void* data,
                               struct wl_output* wl_output,
                               uint32_t flags,
                               int32_t width,
                               int32_t height,
                               int32_t refresh) {
  if ((WL_OUTPUT_MODE_CURRENT & flags) != WL_OUTPUT_MODE_CURRENT)
    return;

  size_.SetSize(width, height);
  switch (transform_) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
    case WL_OUTPUT_TRANSFORM_180:
      surface_size_.SetSize(width, height);
      break;
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_270:
      surface_size_.SetSize(height, width);
      break;
    default:
      NOTREACHED();
      break;
  }

  std::unique_ptr<wl_region> opaque_region(static_cast<wl_region*>(
      wl_compositor_create_region(globals_.compositor.get())));

  if (!opaque_region) {
    LOG(ERROR) << "Can't create region";
    return;
  }

  wl_region_add(opaque_region.get(), 0, 0, surface_size_.width(),
                surface_size_.height());
  wl_surface_set_opaque_region(surface_.get(), opaque_region.get());
  wl_surface_set_input_region(surface_.get(), opaque_region.get());
}

void WebviewClient::InputCallback() {
  std::string request;
  std::cin >> request;

  if (request == "q") {
    run_loop_.Quit();
    return;
  }

  SendNavigationRequest(request);
  std::cout << "Enter URL, q to quit: ";
  std::cout.flush();
}

void WebviewClient::Paint() {
  Buffer* buffer = DequeueBuffer();

  if (!buffer)
    return;

  if (gr_context_) {
    gr_context_->flush();
    glFinish();
  }

  wl_surface_set_buffer_scale(surface_.get(), scale_);
  wl_surface_set_buffer_transform(surface_.get(), transform_);
  wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                    surface_size_.height());
  wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

  wl_surface_set_buffer_scale(webview_surface_.get(), scale_);
  wl_surface_damage(webview_surface_.get(), 0, 0, surface_size_.width(),
                    surface_size_.height());
  wl_surface_attach(webview_surface_.get(), webview_buffer_->buffer.get(), 0,
                    0);

  // Set up frame callbacks.
  frame_callback_.reset(wl_surface_frame(surface_.get()));
  static wl_callback_listener frame_listener = {FrameCallback};
  wl_callback_add_listener(frame_callback_.get(), &frame_listener, this);

  wl_surface_commit(webview_surface_.get());
  wl_surface_commit(surface_.get());
  wl_display_flush(display_.get());
}

void WebviewClient::SchedulePaint() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebviewClient::Paint, base::Unretained(this)));
}

void WebviewClient::SendNavigationRequest(const std::string& URL) {
  WebviewRequest load_url_request;
  load_url_request.mutable_navigate()->set_url(URL);

  if (!client_->Write(load_url_request)) {
    LOG(ERROR) << ("Navigation request send failed");
  }
}

void WebviewClient::TakeExclusiveAccess() {
  while (wl_display_prepare_read(display_.get()) == -1) {
    if (wl_display_dispatch_pending(display_.get()) == -1) {
      LOG(ERROR) << "Error dispatching Wayland events";
      return;
    }
  }
  wl_display_flush(display_.get());
}

void WebviewClient::WlDisplayCallback() {
  wl_display_read_events(display_.get());
  TakeExclusiveAccess();
}

}  // namespace client
}  // namespace chromecast
