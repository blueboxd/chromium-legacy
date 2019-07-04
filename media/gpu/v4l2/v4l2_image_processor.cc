// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/scopedfd_helper.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_image_processor.h"

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value, type_str) \
  do {                                                          \
    if (device_->Ioctl(type, arg) != 0) {                       \
      VPLOGF(1) << "ioctl() failed: " << type_str;              \
      return value;                                             \
    }                                                           \
  } while (0)

#define IOCTL_OR_ERROR_RETURN(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, ((void)0), #type)

#define IOCTL_OR_ERROR_RETURN_FALSE(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, false, #type)

#define IOCTL_OR_LOG_ERROR(type, arg)           \
  do {                                          \
    if (device_->Ioctl(type, arg) != 0)         \
      VPLOGF(1) << "ioctl() failed: " << #type; \
  } while (0)

namespace media {

V4L2ImageProcessor::JobRecord::JobRecord() = default;

V4L2ImageProcessor::JobRecord::~JobRecord() = default;

V4L2ImageProcessor::V4L2ImageProcessor(
    scoped_refptr<V4L2Device> device,
    VideoFrame::StorageType input_storage_type,
    VideoFrame::StorageType output_storage_type,
    v4l2_memory input_memory_type,
    v4l2_memory output_memory_type,
    OutputMode output_mode,
    const VideoFrameLayout& input_layout,
    const VideoFrameLayout& output_layout,
    gfx::Size input_visible_size,
    gfx::Size output_visible_size,
    size_t num_buffers,
    ErrorCB error_cb)
    : ImageProcessor(input_layout,
                     input_storage_type,
                     output_layout,
                     output_storage_type,
                     output_mode),
      input_visible_size_(input_visible_size),
      input_memory_type_(input_memory_type),
      output_visible_size_(output_visible_size),
      output_memory_type_(output_memory_type),
      device_(device),
      device_thread_("V4L2ImageProcessorThread"),
      device_poll_thread_("V4L2ImageProcessorDevicePollThread"),
      num_buffers_(num_buffers),
      error_cb_(error_cb),
      weak_this_factory_(this) {
  DETACH_FROM_THREAD(device_thread_checker_);
}

V4L2ImageProcessor::~V4L2ImageProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  Destroy();

  DCHECK(!device_thread_.IsRunning());
  DCHECK(!device_poll_thread_.IsRunning());
}

void V4L2ImageProcessor::NotifyError() {
  VLOGF(1);
  error_cb_.Run();
}

namespace {

v4l2_memory InputStorageTypeToV4L2Memory(VideoFrame::StorageType storage_type) {
  switch (storage_type) {
    case VideoFrame::STORAGE_OWNED_MEMORY:
    case VideoFrame::STORAGE_UNOWNED_MEMORY:
    case VideoFrame::STORAGE_SHMEM:
    case VideoFrame::STORAGE_MOJO_SHARED_BUFFER:
      return V4L2_MEMORY_USERPTR;
    case VideoFrame::STORAGE_DMABUFS:
      return V4L2_MEMORY_DMABUF;
    default:
      return static_cast<v4l2_memory>(0);
  }
}

}  // namespace

// static
std::unique_ptr<V4L2ImageProcessor> V4L2ImageProcessor::Create(
    scoped_refptr<V4L2Device> device,
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    const ImageProcessor::OutputMode output_mode,
    size_t num_buffers,
    ErrorCB error_cb) {
  VLOGF(2);
  DCHECK_GT(num_buffers, 0u);
  if (!device) {
    VLOGF(2) << "Failed creating V4L2Device";
    return nullptr;
  }

  // V4L2ImageProcessor supports either DmaBuf-backed or memory-based video
  // frame for input.
  VideoFrame::StorageType input_storage_type = VideoFrame::STORAGE_UNKNOWN;
  for (auto input_type : input_config.preferred_storage_types) {
    if (input_type == VideoFrame::STORAGE_DMABUFS ||
        VideoFrame::IsStorageTypeMappable(input_type)) {
      input_storage_type = input_type;
      break;
    }
  }
  if (input_storage_type == VideoFrame::STORAGE_UNKNOWN) {
    VLOGF(2) << "Unsupported input storage type";
    return nullptr;
  }

  // V4L2ImageProcessor only supports DmaBuf-backed video frame for output.
  VideoFrame::StorageType output_storage_type = VideoFrame::STORAGE_UNKNOWN;
  for (auto output_type : output_config.preferred_storage_types) {
    if (output_type == VideoFrame::STORAGE_DMABUFS) {
      output_storage_type = output_type;
      break;
    }
  }
  if (output_storage_type == VideoFrame::STORAGE_UNKNOWN) {
    VLOGF(2) << "Unsupported output storage type";
    return nullptr;
  }

  const v4l2_memory input_memory_type = InputStorageTypeToV4L2Memory(
      input_storage_type);
  if (input_memory_type == 0) {
    VLOGF(1) << "Unsupported input storage type: " << input_storage_type;
    return nullptr;
  }

  const v4l2_memory output_memory_type =
      output_mode == ImageProcessor::OutputMode::ALLOCATE ? V4L2_MEMORY_MMAP
                                                          : V4L2_MEMORY_DMABUF;

  if (!device->IsImageProcessingSupported()) {
    VLOGF(1) << "V4L2ImageProcessor not supported in this platform";
    return nullptr;
  }

  const VideoFrameLayout& input_layout = input_config.layout;
  const uint32_t input_format_fourcc =
      V4L2Device::VideoFrameLayoutToV4L2PixFmt(input_layout);
  if (!input_format_fourcc) {
    VLOGF(1) << "Invalid VideoFrameLayout: " << input_layout;
    return nullptr;
  }
  if (!device->Open(V4L2Device::Type::kImageProcessor, input_format_fourcc)) {
    VLOGF(1) << "Failed to open device for input format: "
             << VideoPixelFormatToString(input_layout.format())
             << " fourcc: " << FourccToString(input_format_fourcc);
    return nullptr;
  }

  // Try to set input format.
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.width = input_layout.coded_size().width();
  format.fmt.pix_mp.height = input_layout.coded_size().height();
  format.fmt.pix_mp.pixelformat = input_format_fourcc;
  if (device->Ioctl(VIDIOC_S_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != input_format_fourcc) {
    VLOGF(1) << "Failed to negotiate input format";
    return nullptr;
  }

  base::Optional<VideoFrameLayout> negotiated_input_layout =
      V4L2Device::V4L2FormatToVideoFrameLayout(format);
  if (!negotiated_input_layout) {
    VLOGF(1) << "Failed to negotiate input VideoFrameLayout";
    return nullptr;
  }

  if (!gfx::Rect(negotiated_input_layout->coded_size())
           .Contains(gfx::Rect(input_config.visible_size))) {
    VLOGF(1) << "Negotiated input allocated size: "
             << negotiated_input_layout->coded_size().ToString()
             << " should contain visible size: "
             << input_config.visible_size.ToString();
    return nullptr;
  }

  const VideoFrameLayout& output_layout = output_config.layout;
  const uint32_t output_format_fourcc =
      V4L2Device::VideoFrameLayoutToV4L2PixFmt(output_layout);
  if (!output_format_fourcc) {
    VLOGF(1) << "Invalid VideoFrameLayout: " << output_layout;
    return nullptr;
  }

  // Try to set output format.
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width = output_layout.coded_size().width();
  format.fmt.pix_mp.height = output_layout.coded_size().height();
  format.fmt.pix_mp.pixelformat = output_format_fourcc;
  format.fmt.pix_mp.num_planes =
      V4L2Device::GetNumPlanesOfV4L2PixFmt(output_format_fourcc);
  for (size_t i = 0; i < format.fmt.pix_mp.num_planes; ++i) {
    format.fmt.pix_mp.plane_fmt[i].sizeimage = output_layout.planes()[i].size;
    format.fmt.pix_mp.plane_fmt[i].bytesperline =
        output_layout.planes()[i].stride;
  }
  if (device->Ioctl(VIDIOC_S_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != output_format_fourcc) {
    VLOGF(1) << "Failed to negotiate output format";
    return nullptr;
  }
  base::Optional<VideoFrameLayout> negotiated_output_layout =
      V4L2Device::V4L2FormatToVideoFrameLayout(format);
  if (!negotiated_output_layout) {
    VLOGF(1) << "Failed to negotiate output VideoFrameLayout";
    return nullptr;
  }
  if (!gfx::Rect(negotiated_output_layout->coded_size())
           .Contains(gfx::Rect(output_layout.coded_size()))) {
    VLOGF(1) << "Negotiated output allocated size: "
             << negotiated_output_layout->coded_size().ToString()
             << " should contain original output allocated size: "
             << output_layout.coded_size().ToString();
    return nullptr;
  }

  auto processor = base::WrapUnique(new V4L2ImageProcessor(
      std::move(device), input_storage_type, output_storage_type,
      input_memory_type, output_memory_type, output_mode,
      *negotiated_input_layout, *negotiated_output_layout,
      input_config.visible_size, output_config.visible_size, num_buffers,
      media::BindToCurrentLoop(std::move(error_cb))));
  if (!processor->Initialize()) {
    VLOGF(1) << "Failed to initialize V4L2ImageProcessor";
    return nullptr;
  }
  return processor;
}

bool V4L2ImageProcessor::Initialize() {
  // Capabilities check.
  struct v4l2_capability caps;
  memset(&caps, 0, sizeof(caps));
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYCAP, &caps);
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    VLOGF(1) << "Initialize(): ioctl() failed: VIDIOC_QUERYCAP: "
             << "caps check failed: 0x" << std::hex << caps.capabilities;
    return false;
  }

  if (!device_thread_.Start()) {
    VLOGF(1) << "Initialize(): device thread failed to start";
    return false;
  }

  // Call to AllocateBuffers must be asynchronous.
  base::WaitableEvent done;
  bool result;
  device_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessor::AllocateBuffersTask,
                                base::Unretained(this), &result, &done));
  done.Wait();
  if (!result) {
    return false;
  }

  // StartDevicePoll will NotifyError on failure.
  device_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessor::StartDevicePoll,
                                base::Unretained(this)));

  VLOGF(2) << "V4L2ImageProcessor initialized for "
           << "input_layout: " << input_layout_
           << ", output_layout: " << output_layout_
           << ", input_visible_size: " << input_visible_size_.ToString()
           << ", output_visible_size: " << output_visible_size_.ToString();

  return true;
}

// static
bool V4L2ImageProcessor::IsSupported() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return false;

  return device->IsImageProcessingSupported();
}

// static
std::vector<uint32_t> V4L2ImageProcessor::GetSupportedInputFormats() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return std::vector<uint32_t>();

  return device->GetSupportedImageProcessorPixelformats(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
}

// static
std::vector<uint32_t> V4L2ImageProcessor::GetSupportedOutputFormats() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return std::vector<uint32_t>();

  return device->GetSupportedImageProcessorPixelformats(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
}

// static
bool V4L2ImageProcessor::TryOutputFormat(uint32_t input_pixelformat,
                                         uint32_t output_pixelformat,
                                         gfx::Size* size,
                                         size_t* num_planes) {
  DVLOGF(3) << "input=" << FourccToString(input_pixelformat)
            << " output=" << FourccToString(output_pixelformat)
            << " size=" << size->ToString();
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device ||
      !device->Open(V4L2Device::Type::kImageProcessor, input_pixelformat))
    return false;

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width = size->width();
  format.fmt.pix_mp.height = size->height();
  format.fmt.pix_mp.pixelformat = output_pixelformat;
  if (device->Ioctl(VIDIOC_TRY_FMT, &format) != 0)
    return false;

  *num_planes = format.fmt.pix_mp.num_planes;
  *size = V4L2Device::AllocatedSizeFromV4L2Format(format);
  DVLOGF(3) << "adjusted output coded size=" << size->ToString()
            << ", num_planes=" << *num_planes;
  return true;
}

bool V4L2ImageProcessor::ProcessInternal(
    scoped_refptr<VideoFrame> frame,
    LegacyFrameReadyCB cb) {
  DVLOGF(4) << "ts=" << frame->timestamp().InMilliseconds();

  auto job_record = std::make_unique<JobRecord>();
  job_record->input_frame = frame;
  job_record->legacy_ready_cb = std::move(cb);

  if (output_memory_type_ != V4L2_MEMORY_MMAP) {
    NOTREACHED();
  }

  // Since device_thread_ is owned by this class. base::Unretained(this) and the
  // raw pointer of that task runner are safe.
  process_task_tracker_.PostTask(
      device_thread_.task_runner().get(), FROM_HERE,
      base::BindOnce(&V4L2ImageProcessor::ProcessTask, base::Unretained(this),
                     std::move(job_record)));
  return true;
}

bool V4L2ImageProcessor::ProcessInternal(scoped_refptr<VideoFrame> input_frame,
                                         scoped_refptr<VideoFrame> output_frame,
                                         FrameReadyCB cb) {
  DVLOGF(4) << "ts=" << input_frame->timestamp().InMilliseconds();

  auto job_record = std::make_unique<JobRecord>();
  job_record->input_frame = std::move(input_frame);
  job_record->output_frame = std::move(output_frame);
  job_record->ready_cb = std::move(cb);

  process_task_tracker_.PostTask(
      device_thread_.task_runner().get(), FROM_HERE,
      base::BindOnce(&V4L2ImageProcessor::ProcessTask, base::Unretained(this),
                     std::move(job_record)));
  return true;
}

void V4L2ImageProcessor::ProcessTask(std::unique_ptr<JobRecord> job_record) {
  DCHECK_CALLED_ON_VALID_THREAD(device_thread_checker_);

  input_job_queue_.emplace(std::move(job_record));
  ProcessJobsTask();
}

void V4L2ImageProcessor::ProcessJobsTask() {
  DCHECK_CALLED_ON_VALID_THREAD(device_thread_checker_);

  while (!input_job_queue_.empty()) {
    // We need one input and one output buffer to schedule the job
    if (input_queue_->FreeBuffersCount() == 0 ||
        output_queue_->FreeBuffersCount() == 0)
      break;

    auto job_record = std::move(input_job_queue_.front());
    input_job_queue_.pop();
    EnqueueInput(job_record.get());
    EnqueueOutput(job_record.get());
    running_jobs_.emplace(std::move(job_record));
  }
}

bool V4L2ImageProcessor::Reset() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(device_thread_.IsRunning());

  process_task_tracker_.TryCancelAll();
  return true;
}

void V4L2ImageProcessor::Destroy() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  // If the device thread is running, destroy using posted task.
  if (device_thread_.IsRunning()) {
    process_task_tracker_.TryCancelAll();

    device_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&V4L2ImageProcessor::StopDevicePoll,
                                  base::Unretained(this)));
    device_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&V4L2ImageProcessor::DestroyBuffersTask,
                                  base::Unretained(this)));
    // Wait for tasks to finish/early-exit.
    device_thread_.Stop();
  } else {
    // Otherwise DestroyTask() is not needed.
    DCHECK(!device_poll_thread_.IsRunning());
  }
}

bool V4L2ImageProcessor::CreateInputBuffers() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_THREAD(device_thread_checker_);
  DCHECK_EQ(input_queue_, nullptr);

  struct v4l2_control control;
  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_ROTATE;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_HFLIP;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_VFLIP;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_ALPHA_COMPONENT;
  control.value = 255;
  if (device_->Ioctl(VIDIOC_S_CTRL, &control) != 0)
    DVLOGF(4) << "V4L2_CID_ALPHA_COMPONENT is not supported";

  struct v4l2_rect visible_rect;
  visible_rect.left = 0;
  visible_rect.top = 0;
  visible_rect.width = base::checked_cast<__u32>(input_visible_size_.width());
  visible_rect.height = base::checked_cast<__u32>(input_visible_size_.height());

  struct v4l2_selection selection_arg;
  memset(&selection_arg, 0, sizeof(selection_arg));
  selection_arg.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  selection_arg.target = V4L2_SEL_TGT_CROP;
  selection_arg.r = visible_rect;
  if (device_->Ioctl(VIDIOC_S_SELECTION, &selection_arg) != 0) {
    VLOGF(2) << "Fallback to VIDIOC_S_CROP for input buffers.";
    struct v4l2_crop crop;
    memset(&crop, 0, sizeof(crop));
    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    crop.c = visible_rect;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CROP, &crop);
  }

  input_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  if (!input_queue_)
    return false;

  if (input_queue_->AllocateBuffers(num_buffers_, input_memory_type_) == 0u)
    return false;

  if (input_queue_->AllocatedBuffersCount() != num_buffers_) {
    VLOGF(1) << "Failed to allocate the required number of input buffers. "
             << "Requested " << num_buffers_ << ", got "
             << input_queue_->AllocatedBuffersCount() << ".";
    return false;
  }

  return true;
}

bool V4L2ImageProcessor::CreateOutputBuffers() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_THREAD(device_thread_checker_);
  DCHECK_EQ(output_queue_, nullptr);

  struct v4l2_rect visible_rect;
  visible_rect.left = 0;
  visible_rect.top = 0;
  visible_rect.width = base::checked_cast<__u32>(output_visible_size_.width());
  visible_rect.height =
    base::checked_cast<__u32>(output_visible_size_.height());

  output_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  if (!output_queue_)
    return false;

  struct v4l2_selection selection_arg;
  memset(&selection_arg, 0, sizeof(selection_arg));
  selection_arg.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  selection_arg.target = V4L2_SEL_TGT_COMPOSE;
  selection_arg.r = visible_rect;
  if (device_->Ioctl(VIDIOC_S_SELECTION, &selection_arg) != 0) {
    VLOGF(2) << "Fallback to VIDIOC_S_CROP for output buffers.";
    struct v4l2_crop crop;
    memset(&crop, 0, sizeof(crop));
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    crop.c = visible_rect;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CROP, &crop);
  }

  if (output_queue_->AllocateBuffers(num_buffers_, output_memory_type_) == 0)
    return false;

  if (output_queue_->AllocatedBuffersCount() != num_buffers_) {
    VLOGF(1) << "Failed to allocate output buffers. Allocated number="
             << output_queue_->AllocatedBuffersCount()
             << ", Requested number=" << num_buffers_;
    return false;
  }

  return true;
}

void V4L2ImageProcessor::DevicePollTask(bool poll_device) {
  DVLOGF(4);
  DCHECK(device_poll_thread_.task_runner()->BelongsToCurrentThread());

  bool event_pending;
  if (!device_->Poll(poll_device, &event_pending)) {
    NotifyError();
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch processor state from this thread.
  device_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessor::ServiceDeviceTask,
                                base::Unretained(this)));
}

void V4L2ImageProcessor::ServiceDeviceTask() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_THREAD(device_thread_checker_);
  // ServiceDeviceTask() should only ever be scheduled from DevicePollTask(),
  // so either:
  // * device_poll_thread_ is running normally
  // * device_poll_thread_ scheduled us, but then a DestroyTask() shut it down,
  //   in which case we should early-out.
  if (!device_poll_thread_.task_runner())
    return;

  DCHECK(input_queue_);

  Dequeue();
  ProcessJobsTask();

  if (!device_->ClearDevicePollInterrupt()) {
    NotifyError();
    return;
  }

  bool poll_device = (input_queue_->QueuedBuffersCount() > 0 ||
                      output_queue_->QueuedBuffersCount() > 0);

  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessor::DevicePollTask,
                                base::Unretained(this), poll_device));

  DVLOGF(3) << __func__ << ": buffer counts: INPUT[" << input_job_queue_.size()
            << "] => DEVICE[" << input_queue_->FreeBuffersCount() << "+"
            << input_queue_->QueuedBuffersCount() << "/"
            << input_queue_->AllocatedBuffersCount() << "->"
            << output_queue_->AllocatedBuffersCount() -
                   output_queue_->QueuedBuffersCount()
            << "+" << output_queue_->QueuedBuffersCount() << "/"
            << output_queue_->AllocatedBuffersCount() << "]";
}

void V4L2ImageProcessor::EnqueueInput(const JobRecord* job_record) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_THREAD(device_thread_checker_);
  DCHECK(input_queue_);

  const size_t old_inputs_queued = input_queue_->QueuedBuffersCount();
  if (!EnqueueInputRecord(job_record))
    return;

  if (old_inputs_queued == 0 && input_queue_->QueuedBuffersCount() != 0) {
    // We started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt()) {
      NotifyError();
      return;
    }
    // VIDIOC_STREAMON if we haven't yet.
    if (!input_queue_->Streamon())
      return;
  }
}

void V4L2ImageProcessor::EnqueueOutput(const JobRecord* job_record) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_THREAD(device_thread_checker_);
  DCHECK(output_queue_);

  const int old_outputs_queued = output_queue_->QueuedBuffersCount();
  if (!EnqueueOutputRecord(job_record))
    return;

  if (old_outputs_queued == 0 && output_queue_->QueuedBuffersCount() != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt()) {
      NotifyError();
      return;
    }
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!output_queue_->Streamon())
      return;
  }
}

void V4L2ImageProcessor::V4L2VFDestructionObserver(V4L2ReadableBufferRef buf) {
  DCHECK(device_thread_.task_runner()->BelongsToCurrentThread());

  // Release the buffer reference so we can directly call ProcessJobsTask()
  // knowing that we have an extra output buffer.
#if DCHECK_IS_ON()
  size_t original_free_buffers_count = output_queue_->FreeBuffersCount();
#endif
  buf = nullptr;
#if DCHECK_IS_ON()
  DCHECK_EQ(output_queue_->FreeBuffersCount(), original_free_buffers_count + 1);
#endif

  // A CAPTURE buffer has just been returned to the free list, let's see if
  // we can make progress on some jobs.
  ProcessJobsTask();
}

void V4L2ImageProcessor::Dequeue() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_THREAD(device_thread_checker_);
  DCHECK(input_queue_);
  DCHECK(output_queue_);
  DCHECK(input_queue_->IsStreaming());

  // Dequeue completed input (VIDEO_OUTPUT) buffers,
  // and recycle to the free list.
  while (input_queue_->QueuedBuffersCount() > 0) {
    bool res;
    V4L2ReadableBufferRef buffer;
    std::tie(res, buffer) = input_queue_->DequeueBuffer();
    if (!res) {
      NotifyError();
      return;
    }
    if (!buffer) {
      // No error occurred, we are just out of buffers to dequeue.
      break;
    }
  }

  // Dequeue completed output (VIDEO_CAPTURE) buffers.
  // Return the finished buffer to the client via the job ready callback.
  while (output_queue_->QueuedBuffersCount() > 0) {
    DCHECK(output_queue_->IsStreaming());

    bool res;
    V4L2ReadableBufferRef buffer;
    std::tie(res, buffer) = output_queue_->DequeueBuffer();
    if (!res) {
      NotifyError();
      return;
    } else if (!buffer) {
      break;
    }

    // Jobs are always processed in FIFO order.
    DCHECK(!running_jobs_.empty());
    std::unique_ptr<JobRecord> job_record = std::move(running_jobs_.front());
    running_jobs_.pop();

    scoped_refptr<VideoFrame> output_frame;
    switch (output_memory_type_) {
      case V4L2_MEMORY_MMAP:
        // Wrap the V4L2 VideoFrame into another one with a destruction observer
        // so we can reuse the MMAP buffer once the client is done with it.
        {
          const auto& orig_frame = buffer->GetVideoFrame();
          output_frame = VideoFrame::WrapVideoFrame(
              *orig_frame, orig_frame->format(), orig_frame->visible_rect(),
              orig_frame->natural_size());
          output_frame->AddDestructionObserver(BindToCurrentLoop(
              base::BindOnce(&V4L2ImageProcessor::V4L2VFDestructionObserver,
                             weak_this_factory_.GetWeakPtr(), buffer)));
        }
        break;

      case V4L2_MEMORY_DMABUF:
        output_frame = std::move(job_record->output_frame);
        break;

      default:
        NOTREACHED();
        return;
    }

    output_frame->set_timestamp(job_record->input_frame->timestamp());

    if (!job_record->legacy_ready_cb.is_null()) {
      std::move(job_record->legacy_ready_cb)
          .Run(buffer->BufferId(), std::move(output_frame));
    } else {
      std::move(job_record->ready_cb).Run(std::move(output_frame));
    }
  }
}

bool V4L2ImageProcessor::EnqueueInputRecord(const JobRecord* job_record) {
  DVLOGF(4);
  DCHECK(input_queue_);
  DCHECK_GT(input_queue_->FreeBuffersCount(), 0u);

  V4L2WritableBufferRef buffer(input_queue_->GetFreeBuffer());
  DCHECK(buffer.IsValid());

  std::vector<void*> user_ptrs;
  size_t num_planes = V4L2Device::GetNumPlanesOfV4L2PixFmt(
      V4L2Device::VideoFrameLayoutToV4L2PixFmt(input_layout_));
  for (size_t i = 0; i < num_planes; ++i) {
    int bytes_used = VideoFrame::PlaneSize(job_record->input_frame->format(), i,
                                           input_layout_.coded_size())
                         .GetArea();
    buffer.SetPlaneBytesUsed(i, bytes_used);
    if (buffer.Memory() == V4L2_MEMORY_USERPTR)
      user_ptrs.push_back(job_record->input_frame->data(i));
  }

  switch (input_memory_type_) {
    case V4L2_MEMORY_USERPTR:
      std::move(buffer).QueueUserPtr(user_ptrs);
      break;
    case V4L2_MEMORY_DMABUF:
      std::move(buffer).QueueDMABuf(job_record->input_frame->DmabufFds());
      break;
    default:
      NOTREACHED();
      return false;
  }
  DVLOGF(4) << "enqueued frame ts="
            << job_record->input_frame->timestamp().InMilliseconds()
            << " to device.";

  return true;
}

bool V4L2ImageProcessor::EnqueueOutputRecord(const JobRecord* job_record) {
  DVLOGF(4);
  DCHECK_GT(output_queue_->FreeBuffersCount(), 0u);

  V4L2WritableBufferRef buffer(output_queue_->GetFreeBuffer());
  DCHECK(buffer.IsValid());

  switch (buffer.Memory()) {
    case V4L2_MEMORY_MMAP:
      return std::move(buffer).QueueMMap();
    case V4L2_MEMORY_DMABUF:
      return std::move(buffer).QueueDMABuf(
          job_record->output_frame->DmabufFds());
    default:
      NOTREACHED();
      return false;
  }
}

void V4L2ImageProcessor::AllocateBuffersTask(bool* result,
                                             base::WaitableEvent* done) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_THREAD(device_thread_checker_);

  *result = CreateInputBuffers() && CreateOutputBuffers();
  done->Signal();
}

void V4L2ImageProcessor::DestroyBuffersTask() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_THREAD(device_thread_checker_);

  weak_this_factory_.InvalidateWeakPtrs();

  // We may be destroyed before we allocate any buffer.
  if (input_queue_)
    input_queue_->DeallocateBuffers();
  if (output_queue_)
    output_queue_->DeallocateBuffers();
  input_queue_ = nullptr;
  output_queue_ = nullptr;
}

void V4L2ImageProcessor::StartDevicePoll() {
  DVLOGF(3) << "starting device poll";
  DCHECK_CALLED_ON_VALID_THREAD(device_thread_checker_);
  DCHECK(!device_poll_thread_.IsRunning());

  // Start up the device poll thread and schedule its first DevicePollTask().
  if (!device_poll_thread_.Start()) {
    VLOGF(1) << "StartDevicePoll(): Device thread failed to start";
    NotifyError();
    return;
  }
  // Enqueue a poll task with no devices to poll on - will wait only for the
  // poll interrupt
  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessor::DevicePollTask,
                                base::Unretained(this), false));
}

void V4L2ImageProcessor::StopDevicePoll() {
  DVLOGF(3) << "stopping device poll";
  DCHECK_CALLED_ON_VALID_THREAD(device_thread_checker_);

  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  bool result = device_->SetDevicePollInterrupt();
  device_poll_thread_.Stop();
  if (!result) {
    NotifyError();
    return;
  }

  // Clear the interrupt now, to be sure.
  if (!device_->ClearDevicePollInterrupt()) {
    NotifyError();
    return;
  }

  if (input_queue_)
    input_queue_->Streamoff();

  if (output_queue_)
    output_queue_->Streamoff();

  // Reset all our accounting info.
  while (!input_job_queue_.empty())
    input_job_queue_.pop();

  while (!running_jobs_.empty())
    running_jobs_.pop();
}

}  // namespace media
