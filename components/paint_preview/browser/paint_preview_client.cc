// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_client.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace paint_preview {

namespace {

// Creates an old style id of Process ID || Routing ID. This should only be used
// for looking up the main frame's filler GUID in cases where only the
// RenderFrameHost is available (such as in RenderFrameDeleted()).
uint64_t MakeOldStyleId(content::RenderFrameHost* render_frame_host) {
  return (static_cast<uint64_t>(render_frame_host->GetProcess()->GetID())
          << 32) |
         render_frame_host->GetRoutingID();
}

// Converts gfx::Rect to its RectProto form.
void RectToRectProto(const gfx::Rect& rect, RectProto* proto) {
  proto->set_x(rect.x());
  proto->set_y(rect.y());
  proto->set_width(rect.width());
  proto->set_height(rect.height());
}

// Converts |response| into |proto|. Returns a list of the frame GUIDs
// referenced by the response.
std::vector<base::UnguessableToken>
PaintPreviewCaptureResponseToPaintPreviewFrameProto(
    mojom::PaintPreviewCaptureResponsePtr response,
    base::UnguessableToken frame_guid,
    PaintPreviewFrameProto* proto) {
  proto->set_embedding_token_high(frame_guid.GetHighForSerialization());
  proto->set_embedding_token_low(frame_guid.GetLowForSerialization());

  std::vector<base::UnguessableToken> frame_guids;
  for (const auto& id_pair : response->content_id_to_embedding_token) {
    auto* content_id_embedding_token_pair =
        proto->add_content_id_to_embedding_tokens();
    content_id_embedding_token_pair->set_content_id(id_pair.first);
    content_id_embedding_token_pair->set_embedding_token_low(
        id_pair.second.GetLowForSerialization());
    content_id_embedding_token_pair->set_embedding_token_high(
        id_pair.second.GetHighForSerialization());
    frame_guids.push_back(id_pair.second);
  }

  for (const auto& link : response->links) {
    auto* link_proto = proto->add_links();
    link_proto->set_url(link->url.spec());
    RectToRectProto(link->rect, link_proto->mutable_rect());
  }

  return frame_guids;
}

}  // namespace

PaintPreviewClient::PaintPreviewParams::PaintPreviewParams() = default;
PaintPreviewClient::PaintPreviewParams::~PaintPreviewParams() = default;

PaintPreviewClient::PaintPreviewData::PaintPreviewData() = default;
PaintPreviewClient::PaintPreviewData::~PaintPreviewData() = default;

PaintPreviewClient::PaintPreviewData& PaintPreviewClient::PaintPreviewData::
operator=(PaintPreviewData&& rhs) noexcept = default;
PaintPreviewClient::PaintPreviewData::PaintPreviewData(
    PaintPreviewData&& other) noexcept = default;

PaintPreviewClient::CreateResult::CreateResult(base::File file,
                                               base::File::Error error)
    : file(std::move(file)), error(error) {}

PaintPreviewClient::CreateResult::~CreateResult() = default;

PaintPreviewClient::CreateResult::CreateResult(CreateResult&& other) = default;
PaintPreviewClient::CreateResult& PaintPreviewClient::CreateResult::operator=(
    CreateResult&& other) = default;

PaintPreviewClient::PaintPreviewClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}
PaintPreviewClient::~PaintPreviewClient() = default;

void PaintPreviewClient::CapturePaintPreview(
    const PaintPreviewParams& params,
    content::RenderFrameHost* render_frame_host,
    PaintPreviewCallback callback) {
  if (base::Contains(all_document_data_, params.document_guid)) {
    std::move(callback).Run(params.document_guid,
                            mojom::PaintPreviewStatus::kGuidCollision, nullptr);
    return;
  }
  all_document_data_.insert({params.document_guid, PaintPreviewData()});
  auto* document_data = &all_document_data_[params.document_guid];
  document_data->root_dir = params.root_dir;
  document_data->callback = std::move(callback);
  document_data->root_url = render_frame_host->GetLastCommittedURL();
  CapturePaintPreviewInternal(params, render_frame_host);
}

void PaintPreviewClient::CaptureSubframePaintPreview(
    const base::UnguessableToken& guid,
    const gfx::Rect& rect,
    content::RenderFrameHost* render_subframe_host) {
  PaintPreviewParams params;
  params.document_guid = guid;
  params.clip_rect = rect;
  params.is_main_frame = false;
  CapturePaintPreviewInternal(params, render_subframe_host);
}

void PaintPreviewClient::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // TODO(crbug/1044983): Investigate possible issues with cleanup if just
  // a single subframe gets deleted.
  auto maybe_token = render_frame_host->GetEmbeddingToken();
  bool is_main_frame = false;
  if (!maybe_token.has_value()) {
    uint64_t old_style_id = MakeOldStyleId(render_frame_host);
    auto it = main_frame_guids_.find(old_style_id);
    if (it == main_frame_guids_.end())
      return;
    maybe_token = it->second;
    is_main_frame = true;
  }
  base::UnguessableToken frame_guid = maybe_token.value();
  auto it = pending_previews_on_subframe_.find(frame_guid);
  if (it == pending_previews_on_subframe_.end())
    return;
  for (const auto& document_guid : it->second) {
    auto data_it = all_document_data_.find(document_guid);
    if (data_it == all_document_data_.end())
      continue;
    data_it->second.awaiting_subframes.erase(frame_guid);
    data_it->second.finished_subframes.insert(frame_guid);
    data_it->second.had_error = true;
    if (data_it->second.awaiting_subframes.empty() || is_main_frame) {
      if (is_main_frame) {
        for (const auto& subframe_guid : data_it->second.awaiting_subframes) {
          auto subframe_docs = pending_previews_on_subframe_[subframe_guid];
          subframe_docs.erase(document_guid);
          if (subframe_docs.empty())
            pending_previews_on_subframe_.erase(subframe_guid);
        }
      }
      interface_ptrs_.erase(frame_guid);
      OnFinished(document_guid, &data_it->second);
    }
  }
  pending_previews_on_subframe_.erase(frame_guid);
}

PaintPreviewClient::CreateResult PaintPreviewClient::CreateFileHandle(
    const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  return CreateResult(std::move(file), file.error_details());
}

mojom::PaintPreviewCaptureParamsPtr PaintPreviewClient::CreateMojoParams(
    const PaintPreviewParams& params,
    base::File file) {
  mojom::PaintPreviewCaptureParamsPtr mojo_params =
      mojom::PaintPreviewCaptureParams::New();
  mojo_params->guid = params.document_guid;
  mojo_params->clip_rect = params.clip_rect;
  mojo_params->is_main_frame = params.is_main_frame;
  mojo_params->file = std::move(file);
  return mojo_params;
}

void PaintPreviewClient::CapturePaintPreviewInternal(
    const PaintPreviewParams& params,
    content::RenderFrameHost* render_frame_host) {
  // Use a frame's embedding token as its GUID. Note that we create a GUID for
  // the main frame so that we can treat it the same as other frames.
  auto token = render_frame_host->GetEmbeddingToken();
  if (params.is_main_frame && !token.has_value()) {
    token = base::UnguessableToken::Create();
    main_frame_guids_.insert(
        {MakeOldStyleId(render_frame_host), token.value()});
  }

  // This should be impossible, but if it happens in a release build just abort.
  if (!token.has_value()) {
    DVLOG(1) << "Error: Attempted to capture a non-main frame without an "
                "embedding token.";
    NOTREACHED();
    return;
  }

  auto* document_data = &all_document_data_[params.document_guid];

  base::UnguessableToken frame_guid = token.value();
  if (params.is_main_frame)
    document_data->root_frame_token = frame_guid;
  // Deduplicate data if a subframe is required multiple times.
  if (base::Contains(document_data->awaiting_subframes, frame_guid) ||
      base::Contains(document_data->finished_subframes, frame_guid))
    return;
  base::FilePath file_path = document_data->root_dir.AppendASCII(
      base::StrCat({frame_guid.ToString(), ".skp"}));
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&CreateFileHandle, file_path),
      base::BindOnce(&PaintPreviewClient::RequestCaptureOnUIThread,
                     weak_ptr_factory_.GetWeakPtr(), params, frame_guid,
                     base::Unretained(render_frame_host), file_path));
}

void PaintPreviewClient::RequestCaptureOnUIThread(
    const PaintPreviewParams& params,
    const base::UnguessableToken& frame_guid,
    content::RenderFrameHost* render_frame_host,
    const base::FilePath& file_path,
    CreateResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* document_data = &all_document_data_[params.document_guid];
  if (result.error != base::File::FILE_OK) {
    // Don't block up the UI thread and answer the callback on a different
    // thread.
    base::PostTask(
        FROM_HERE,
        base::BindOnce(std::move(document_data->callback), params.document_guid,
                       mojom::PaintPreviewStatus::kFileCreationError, nullptr));
    return;
  }

  document_data->awaiting_subframes.insert(frame_guid);
  auto it = pending_previews_on_subframe_.find(frame_guid);
  if (it != pending_previews_on_subframe_.end()) {
    it->second.insert(params.document_guid);
  } else {
    pending_previews_on_subframe_.insert(std::make_pair(
        frame_guid,
        base::flat_set<base::UnguessableToken>({params.document_guid})));
  }

  if (!base::Contains(interface_ptrs_, frame_guid)) {
    interface_ptrs_.insert(
        {frame_guid, mojo::AssociatedRemote<mojom::PaintPreviewRecorder>()});
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        &interface_ptrs_[frame_guid]);
  }
  interface_ptrs_[frame_guid]->CapturePaintPreview(
      CreateMojoParams(params, std::move(result.file)),
      base::BindOnce(&PaintPreviewClient::OnPaintPreviewCapturedCallback,
                     weak_ptr_factory_.GetWeakPtr(), params.document_guid,
                     frame_guid, params.is_main_frame, file_path,
                     base::Unretained(render_frame_host)));
}

void PaintPreviewClient::OnPaintPreviewCapturedCallback(
    const base::UnguessableToken& guid,
    const base::UnguessableToken& frame_guid,
    bool is_main_frame,
    const base::FilePath& filename,
    content::RenderFrameHost* render_frame_host,
    mojom::PaintPreviewStatus status,
    mojom::PaintPreviewCaptureResponsePtr response) {
  // There is no retry logic so always treat a frame as processed regardless of
  // |status|
  MarkFrameAsProcessed(guid, frame_guid);

  if (status == mojom::PaintPreviewStatus::kOk)
    status = RecordFrame(guid, frame_guid, is_main_frame, filename,
                         render_frame_host, std::move(response));
  auto* document_data = &all_document_data_[guid];
  if (status != mojom::PaintPreviewStatus::kOk)
    document_data->had_error = true;

  if (document_data->awaiting_subframes.empty())
    OnFinished(guid, document_data);
}

void PaintPreviewClient::MarkFrameAsProcessed(
    base::UnguessableToken guid,
    const base::UnguessableToken& frame_guid) {
  pending_previews_on_subframe_[frame_guid].erase(guid);
  if (pending_previews_on_subframe_[frame_guid].empty())
    interface_ptrs_.erase(frame_guid);
  all_document_data_[guid].finished_subframes.insert(frame_guid);
  all_document_data_[guid].awaiting_subframes.erase(frame_guid);
}

mojom::PaintPreviewStatus PaintPreviewClient::RecordFrame(
    const base::UnguessableToken& guid,
    const base::UnguessableToken& frame_guid,
    bool is_main_frame,
    const base::FilePath& filename,
    content::RenderFrameHost* render_frame_host,
    mojom::PaintPreviewCaptureResponsePtr response) {
  auto it = all_document_data_.find(guid);
  if (!it->second.proto) {
    it->second.proto = std::make_unique<PaintPreviewProto>();
    it->second.proto->mutable_metadata()->set_url(
        all_document_data_[guid].root_url.spec());
  }

  PaintPreviewProto* proto_ptr = it->second.proto.get();

  PaintPreviewFrameProto* frame_proto;
  if (is_main_frame) {
    frame_proto = proto_ptr->mutable_root_frame();
    frame_proto->set_is_main_frame(true);
    uint64_t old_style_id = MakeOldStyleId(render_frame_host);
    main_frame_guids_.erase(old_style_id);
  } else {
    frame_proto = proto_ptr->add_subframes();
    frame_proto->set_is_main_frame(false);
  }
  // Safe since always HEX.skp.
  frame_proto->set_file_path(filename.AsUTF8Unsafe());

  std::vector<base::UnguessableToken> remote_frame_guids =
      PaintPreviewCaptureResponseToPaintPreviewFrameProto(
          std::move(response), frame_guid, frame_proto);

  for (const auto& remote_frame_guid : remote_frame_guids) {
    if (!base::Contains(it->second.finished_subframes, remote_frame_guid))
      it->second.awaiting_subframes.insert(remote_frame_guid);
  }
  return mojom::PaintPreviewStatus::kOk;
}

void PaintPreviewClient::OnFinished(base::UnguessableToken guid,
                                    PaintPreviewData* document_data) {
  if (!document_data)
    return;

  base::UmaHistogramBoolean("Browser.PaintPreview.Capture.Success",
                            document_data->proto != nullptr);
  if (document_data->proto) {
    base::UmaHistogramCounts100(
        "Browser.PaintPreview.Capture.NumberOfFramesCaptured",
        document_data->finished_subframes.size());

    // At a minimum one frame was captured successfully, it is up to the
    // caller to decide if a partial success is acceptable based on what is
    // contained in the proto.
    base::PostTask(
        FROM_HERE,
        base::BindOnce(std::move(document_data->callback), guid,
                       document_data->had_error
                           ? mojom::PaintPreviewStatus::kPartialSuccess
                           : mojom::PaintPreviewStatus::kOk,
                       std::move(document_data->proto)));
  } else {
    // A proto could not be created indicating all frames failed to capture.
    base::PostTask(FROM_HERE,
                   base::BindOnce(std::move(document_data->callback), guid,
                                  mojom::PaintPreviewStatus::kFailed, nullptr));
  }
  all_document_data_.erase(guid);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaintPreviewClient)

}  // namespace paint_preview
