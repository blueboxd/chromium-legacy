// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_H264_ACCELERATOR_H_
#define MEDIA_GPU_V4L2_V4L2_H264_ACCELERATOR_H_

#include <linux/videodev2.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/h264_dpb.h"

namespace media {

class V4L2Device;
class V4L2DecodeSurface;
class V4L2DecodeSurfaceHandler;

class V4L2H264Accelerator : public H264Decoder::H264Accelerator {
 public:
  using Status = H264Decoder::H264Accelerator::Status;

  explicit V4L2H264Accelerator(V4L2DecodeSurfaceHandler* surface_handler,
                               V4L2Device* device);
  ~V4L2H264Accelerator() override;

  // H264Decoder::H264Accelerator implementation.
  scoped_refptr<H264Picture> CreateH264Picture() override;
  Status SubmitFrameMetadata(const H264SPS* sps,
                             const H264PPS* pps,
                             const H264DPB& dpb,
                             const H264Picture::Vector& ref_pic_listp0,
                             const H264Picture::Vector& ref_pic_listb0,
                             const H264Picture::Vector& ref_pic_listb1,
                             scoped_refptr<H264Picture> pic) override;
  Status SubmitSlice(const H264PPS* pps,
                     const H264SliceHeader* slice_hdr,
                     const H264Picture::Vector& ref_pic_list0,
                     const H264Picture::Vector& ref_pic_list1,
                     scoped_refptr<H264Picture> pic,
                     const uint8_t* data,
                     size_t size,
                     const std::vector<SubsampleEntry>& subsamples) override;
  Status SubmitDecode(scoped_refptr<H264Picture> pic) override;
  bool OutputPicture(scoped_refptr<H264Picture> pic) override;
  void Reset() override;

 private:
  // Max size of reference list.
  static constexpr size_t kDPBIndicesListSize = 32;
  // TODO(posciak): This should be queried from hardware once supported.
  static constexpr size_t kMaxSlices = 16;

  void H264PictureListToDPBIndicesList(const H264Picture::Vector& src_pic_list,
                                       uint8_t dst_list[kDPBIndicesListSize]);
  void H264DPBToV4L2DPB(
      const H264DPB& dpb,
      std::vector<scoped_refptr<V4L2DecodeSurface>>* ref_surfaces);
  scoped_refptr<V4L2DecodeSurface> H264PictureToV4L2DecodeSurface(
      H264Picture* pic);

  size_t num_slices_;
  V4L2DecodeSurfaceHandler* const surface_handler_;
  V4L2Device* const device_;

  struct v4l2_ctrl_h264_slice_param v4l2_slice_params_[kMaxSlices];
  struct v4l2_ctrl_h264_decode_param v4l2_decode_param_;

  DISALLOW_COPY_AND_ASSIGN(V4L2H264Accelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_H264_ACCELERATOR_H_
