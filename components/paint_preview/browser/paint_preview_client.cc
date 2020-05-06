// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_client.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
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

uint64_t MakeOldStyleId(const content::GlobalFrameRoutingId& id) {
  return (static_cast<uint64_t>(id.child_id) << 32) | id.frame_routing_id;
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

// Records UKM data for the capture.
// TODO(crbug/1038390): Add more metrics;
// - Peak memory during capture (bucketized).
// - Compressed on disk size (bucketized).
void RecordUkmCaptureData(ukm::SourceId source_id,
                          base::TimeDelta blink_recording_time) {
  if (source_id == ukm::kInvalidSourceId)
    return;
  ukm::builders::PaintPreviewCapture(source_id)
      .SetBlinkCaptureTime(blink_recording_time.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace

PaintPreviewClient::PaintPreviewParams::PaintPreviewParams()
    : is_main_frame(false), max_per_capture_size(0) {}

PaintPreviewClient::PaintPreviewParams::~PaintPreviewParams() = default;

PaintPreviewClient::PaintPreviewData::PaintPreviewData() = default;

PaintPreviewClient::PaintPreviewData::~PaintPreviewData() = default;

PaintPreviewClient::PaintPreviewData&
PaintPreviewClient::PaintPreviewData::operator=(PaintPreviewData&& rhs) =
    default;

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
  PaintPreviewData document_data;
  document_data.root_dir = params.root_dir;
  document_data.callback = std::move(callback);
  document_data.root_url = render_frame_host->GetLastCommittedURL();
  document_data.source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents());
  all_document_data_.insert({params.document_guid, std::move(document_data)});
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "paint_preview", "PaintPreviewClient::CapturePaintPreview",
      TRACE_ID_LOCAL(&all_document_data_[params.document_guid]));
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
    auto* document_data = &data_it->second;
    document_data->awaiting_subframes.erase(frame_guid);
    document_data->finished_subframes.insert(frame_guid);
    document_data->had_error = true;
    if (document_data->awaiting_subframes.empty() || is_main_frame) {
      if (is_main_frame) {
        for (const auto& subframe_guid : document_data->awaiting_subframes) {
          auto subframe_docs = pending_previews_on_subframe_[subframe_guid];
          subframe_docs.erase(document_guid);
          if (subframe_docs.empty())
            pending_previews_on_subframe_.erase(subframe_guid);
        }
      }
      interface_ptrs_.erase(frame_guid);
      OnFinished(document_guid, document_data);
    }
  }
  pending_previews_on_subframe_.erase(frame_guid);
}

PaintPreviewClient::CreateResult PaintPreviewClient::CreateFileHandle(
    const base::FilePath& path) {
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
  mojo_params->max_capture_size = params.max_per_capture_size;
  return mojo_params;
}

void PaintPreviewClient::CapturePaintPreviewInternal(
    const PaintPreviewParams& params,
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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

  auto it = all_document_data_.find(params.document_guid);
  if (it == all_document_data_.end())
    return;
  auto* document_data = &it->second;

  base::UnguessableToken frame_guid = token.value();
  if (params.is_main_frame)
    document_data->root_frame_token = frame_guid;
  // Deduplicate data if a subframe is required multiple times.
  if (base::Contains(document_data->awaiting_subframes, frame_guid) ||
      base::Contains(document_data->finished_subframes, frame_guid))
    return;
  base::FilePath file_path = document_data->root_dir.AppendASCII(
      base::StrCat({frame_guid.ToString(), ".skp"}));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&CreateFileHandle, file_path),
      base::BindOnce(&PaintPreviewClient::RequestCaptureOnUIThread,
                     weak_ptr_factory_.GetWeakPtr(), params, frame_guid,
                     content::GlobalFrameRoutingId(
                         render_frame_host->GetProcess()->GetID(),
                         render_frame_host->GetRoutingID()),
                     file_path));
}

void PaintPreviewClient::RequestCaptureOnUIThread(
    const PaintPreviewParams& params,
    const base::UnguessableToken& frame_guid,
    const content::GlobalFrameRoutingId& render_frame_id,
    const base::FilePath& file_path,
    CreateResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = all_document_data_.find(params.document_guid);
  if (it == all_document_data_.end())
    return;
  auto* document_data = &it->second;
  if (!document_data->callback)
    return;

  if (result.error != base::File::FILE_OK) {
    std::move(document_data->callback)
        .Run(params.document_guid,
             mojom::PaintPreviewStatus::kFileCreationError, nullptr);
    return;
  }

  auto* render_frame_host = content::RenderFrameHost::FromID(render_frame_id);
  if (!render_frame_host) {
    std::move(document_data->callback)
        .Run(params.document_guid, mojom::PaintPreviewStatus::kCaptureFailed,
             nullptr);
    return;
  }

  document_data->awaiting_subframes.insert(frame_guid);
  auto subframe_it = pending_previews_on_subframe_.find(frame_guid);
  if (subframe_it != pending_previews_on_subframe_.end()) {
    subframe_it->second.insert(params.document_guid);
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
                     render_frame_id));
}

void PaintPreviewClient::OnPaintPreviewCapturedCallback(
    const base::UnguessableToken& guid,
    const base::UnguessableToken& frame_guid,
    bool is_main_frame,
    const base::FilePath& filename,
    const content::GlobalFrameRoutingId& render_frame_id,
    mojom::PaintPreviewStatus status,
    mojom::PaintPreviewCaptureResponsePtr response) {
  // There is no retry logic so always treat a frame as processed regardless of
  // |status|
  MarkFrameAsProcessed(guid, frame_guid);

  if (status == mojom::PaintPreviewStatus::kOk)
    status = RecordFrame(guid, frame_guid, is_main_frame, filename,
                         render_frame_id, std::move(response));
  auto it = all_document_data_.find(guid);
  if (it == all_document_data_.end())
    return;
  auto* document_data = &it->second;
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
  auto it = all_document_data_.find(guid);
  if (it == all_document_data_.end())
    return;
  auto* document_data = &it->second;
  document_data->finished_subframes.insert(frame_guid);
  document_data->awaiting_subframes.erase(frame_guid);
}

mojom::PaintPreviewStatus PaintPreviewClient::RecordFrame(
    const base::UnguessableToken& guid,
    const base::UnguessableToken& frame_guid,
    bool is_main_frame,
    const base::FilePath& filename,
    const content::GlobalFrameRoutingId& render_frame_id,
    mojom::PaintPreviewCaptureResponsePtr response) {
  auto it = all_document_data_.find(guid);
  if (it == all_document_data_.end())
    return mojom::PaintPreviewStatus::kCaptureFailed;
  auto* document_data = &it->second;
  if (!document_data->proto) {
    document_data->proto = std::make_unique<PaintPreviewProto>();
    document_data->proto->mutable_metadata()->set_url(
        document_data->root_url.spec());
  }

  PaintPreviewProto* proto_ptr = document_data->proto.get();

  PaintPreviewFrameProto* frame_proto;
  if (is_main_frame) {
    document_data->main_frame_blink_recording_time =
        response->blink_recording_time;
    frame_proto = proto_ptr->mutable_root_frame();
    frame_proto->set_is_main_frame(true);
    uint64_t old_style_id = MakeOldStyleId(render_frame_id);
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
    if (!base::Contains(document_data->finished_subframes, remote_frame_guid))
      document_data->awaiting_subframes.insert(remote_frame_guid);
  }
  return mojom::PaintPreviewStatus::kOk;
}

void PaintPreviewClient::OnFinished(base::UnguessableToken guid,
                                    PaintPreviewData* document_data) {
  if (!document_data || !document_data->callback)
    return;

  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "paint_preview", "PaintPreviewClient::CapturePaintPreview",
      TRACE_ID_LOCAL(document_data), "success", document_data->proto != nullptr,
      "subframes", document_data->finished_subframes.size());

  base::UmaHistogramBoolean("Browser.PaintPreview.Capture.Success",
                            document_data->proto != nullptr);
  if (document_data->proto) {
    base::UmaHistogramCounts100(
        "Browser.PaintPreview.Capture.NumberOfFramesCaptured",
        document_data->finished_subframes.size());

    RecordUkmCaptureData(document_data->source_id,
                         document_data->main_frame_blink_recording_time);

    // At a minimum one frame was captured successfully, it is up to the
    // caller to decide if a partial success is acceptable based on what is
    // contained in the proto.
    std::move(document_data->callback)
        .Run(guid,
             document_data->had_error
                 ? mojom::PaintPreviewStatus::kPartialSuccess
                 : mojom::PaintPreviewStatus::kOk,
             std::move(document_data->proto));
  } else {
    // A proto could not be created indicating all frames failed to capture.
    std::move(document_data->callback)
        .Run(guid, mojom::PaintPreviewStatus::kFailed, nullptr);
  }
  all_document_data_.erase(guid);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaintPreviewClient)

}  // namespace paint_preview
