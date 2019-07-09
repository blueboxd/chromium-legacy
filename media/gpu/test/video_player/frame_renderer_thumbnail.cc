// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/frame_renderer_thumbnail.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/gpu/test/rendering_helper.h"
#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

namespace media {
namespace test {

namespace {

// Size of the large image to which the thumbnails will be rendered.
constexpr gfx::Size kThumbnailsPageSize(1600, 1200);
// Size of the individual thumbnails that will be rendered.
constexpr gfx::Size kThumbnailSize(160, 120);

// Default file path used to store the thumbnail image.
constexpr const base::FilePath::CharType* kDefaultOutputPath =
    FILE_PATH_LITERAL("thumbnail.png");

// Vertex shader used to render thumbnails.
constexpr char kVertexShader[] =
    "varying vec2 interp_tc;\n"
    "attribute vec4 in_pos;\n"
    "attribute vec2 in_tc;\n"
    "uniform bool tex_flip; void main() {\n"
    "  if (tex_flip)\n"
    "    interp_tc = vec2(in_tc.x, 1.0 - in_tc.y);\n"
    "  else\n"
    "   interp_tc = in_tc;\n"
    "  gl_Position = in_pos;\n"
    "}\n";

// Fragment shader used to render thumbnails.
#if !defined(OS_WIN)
constexpr char kFragmentShader[] =
    "#extension GL_OES_EGL_image_external : enable\n"
    "precision mediump float;\n"
    "varying vec2 interp_tc;\n"
    "uniform sampler2D tex;\n"
    "#ifdef GL_OES_EGL_image_external\n"
    "uniform samplerExternalOES tex_external;\n"
    "#endif\n"
    "void main() {\n"
    "  vec4 color = texture2D(tex, interp_tc);\n"
    "#ifdef GL_OES_EGL_image_external\n"
    "  color += texture2D(tex_external, interp_tc);\n"
    "#endif\n"
    "  gl_FragColor = color;\n"
    "}\n";
#else
constexpr char kFragmentShader[] =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec2 interp_tc;\n"
    "uniform sampler2D tex;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(tex, interp_tc);\n"
    "}\n";
#endif

GLuint CreateTexture(GLenum texture_target, const gfx::Size& size) {
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(texture_target, texture_id);
  if (texture_target == GL_TEXTURE_2D) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  }

  glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // OpenGLES2.0.25 section 3.8.2 requires CLAMP_TO_EDGE for NPOT textures.
  glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  return texture_id;
}

// Helper class to automatically acquire and release the GL context.
class AutoGLContext {
 public:
  explicit AutoGLContext(FrameRenderer* const frame_renderer)
      : frame_renderer_(frame_renderer) {
    frame_renderer_->AcquireGLContext();
  }
  ~AutoGLContext() { frame_renderer_->ReleaseGLContext(); }

 private:
  FrameRenderer* const frame_renderer_;
};

}  // namespace

bool FrameRendererThumbnail::gl_initialized_ = false;

FrameRendererThumbnail::FrameRendererThumbnail(
    const std::vector<std::string>& thumbnail_checksums)
    : frame_count_(0),
      thumbnail_checksums_(thumbnail_checksums),
      thumbnails_fbo_id_(0),
      thumbnails_texture_id_(0),
      vertex_buffer_(0),
      program_(0) {
  DETACH_FROM_SEQUENCE(renderer_sequence_checker_);
}

FrameRendererThumbnail::~FrameRendererThumbnail() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  base::AutoLock auto_lock(renderer_lock_);
  DestroyThumbnailImage();
  gl_context_ = nullptr;
  gl_surface_ = nullptr;

  CHECK(mailbox_texture_map_.empty());
}

// static
std::unique_ptr<FrameRendererThumbnail> FrameRendererThumbnail::Create(
    const std::vector<std::string> thumbnail_checksums) {
  auto frame_renderer =
      base::WrapUnique(new FrameRendererThumbnail(thumbnail_checksums));
  frame_renderer->Initialize();
  return frame_renderer;
}

// static
std::unique_ptr<FrameRendererThumbnail> FrameRendererThumbnail::Create(
    const base::FilePath& video_file_path) {
  // Read thumbnail checksums from file.
  std::vector<std::string> thumbnail_checksums =
      media::test::ReadGoldenThumbnailMD5s(
          video_file_path.AddExtension(FILE_PATH_LITERAL(".md5")));

  auto frame_renderer =
      base::WrapUnique(new FrameRendererThumbnail(thumbnail_checksums));
  frame_renderer->Initialize();
  return frame_renderer;
}

void FrameRendererThumbnail::AcquireGLContext() {
  gl_context_lock_.Acquire();
  CHECK(gl_context_->MakeCurrent(gl_surface_.get()));
}

void FrameRendererThumbnail::ReleaseGLContext() {
  gl_context_lock_.AssertAcquired();
  gl_context_->ReleaseCurrent(gl_surface_.get());
  gl_context_lock_.Release();
}

gl::GLContext* FrameRendererThumbnail::GetGLContext() {
  gl_context_lock_.AssertAcquired();
  return gl_context_.get();
}

void FrameRendererThumbnail::RenderFrame(
    scoped_refptr<VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  if (video_frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM))
    return;

  // Find the texture associated with the video frame's mailbox.
  base::AutoLock auto_lock(renderer_lock_);
  const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(0);
  const gpu::Mailbox& mailbox = mailbox_holder.mailbox;
  auto it = mailbox_texture_map_.find(mailbox);
  ASSERT_NE(it, mailbox_texture_map_.end());

  RenderThumbnail(mailbox_holder.texture_target, it->second);
}

void FrameRendererThumbnail::WaitUntilRenderingDone() {}

scoped_refptr<VideoFrame> FrameRendererThumbnail::CreateVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& texture_size,
    uint32_t texture_target,
    uint32_t* texture_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  // Create a mailbox.
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes];
  mailbox_holders[0] =
      gpu::MailboxHolder(mailbox, gpu::SyncToken(), texture_target);

  // Create a new video frame associated with the mailbox.
  base::OnceCallback<void(const gpu::SyncToken&)> mailbox_holder_release_cb =
      BindToCurrentLoop(base::BindOnce(&FrameRendererThumbnail::DeleteTexture,
                                       base::Unretained(this), mailbox));
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
      pixel_format, mailbox_holders, std::move(mailbox_holder_release_cb),
      texture_size, gfx::Rect(texture_size), texture_size, base::TimeDelta());

  // Create a texture and associate it with the mailbox.
  {
    AutoGLContext auto_gl_context(this);
    *texture_id = CreateTexture(texture_target, texture_size);
  }

  base::AutoLock auto_lock(renderer_lock_);
  mailbox_texture_map_.insert(std::make_pair(mailbox, *texture_id));

  return frame;
}

bool FrameRendererThumbnail::ValidateThumbnail() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  base::AutoLock auto_lock(renderer_lock_);
  const std::vector<uint8_t> rgba = ConvertThumbnailToRGBA();

  // Convert the thumbnail from RGBA to RGB.
  std::vector<uint8_t> rgb;
  EXPECT_EQ(media::test::ConvertRGBAToRGB(rgba, &rgb), true)
      << "RGBA frame has incorrect alpha";

  // Calculate the thumbnail's checksum and compare it to golden values.
  std::string md5_string = base::MD5String(
      base::StringPiece(reinterpret_cast<char*>(&rgb[0]), rgb.size()));
  bool is_valid_thumbnail = base::Contains(thumbnail_checksums_, md5_string);
  if (!is_valid_thumbnail)
    SaveThumbnail();

  return is_valid_thumbnail;
}

void FrameRendererThumbnail::SaveThumbnail() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  const std::vector<uint8_t> rgba = ConvertThumbnailToRGBA();

  // Convert raw RGBA into PNG for export.
  std::vector<unsigned char> png;
  gfx::PNGCodec::Encode(&rgba[0], gfx::PNGCodec::FORMAT_RGBA,
                        kThumbnailsPageSize, kThumbnailsPageSize.width() * 4,
                        true, std::vector<gfx::PNGCodec::Comment>(), &png);

  base::FilePath filepath(kDefaultOutputPath);
  int num_bytes =
      base::WriteFile(filepath, reinterpret_cast<char*>(&png[0]), png.size());
  ASSERT_NE(-1, num_bytes);
  EXPECT_EQ(static_cast<size_t>(num_bytes), png.size());
}

void FrameRendererThumbnail::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  // Initialize GL rendering and create GL context.
  if (!gl_initialized_) {
    if (!gl::init::InitializeGLOneOff())
      LOG(FATAL) << "Could not initialize GL";
    gl_initialized_ = true;
  }
  gl_surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
  gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                          gl::GLContextAttribs());

  base::AutoLock auto_lock(renderer_lock_);
  InitializeThumbnailImage();
}

// TODO(dstaessens@) This code is mostly duplicated from
// RenderingHelper::Initialize(), as that code is unfortunately too inflexible
// to reuse here. But most of the code in rendering helper can be removed soon
// when the video_decoder_accelerator_unittests get deprecated.
void FrameRendererThumbnail::InitializeThumbnailImage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  AutoGLContext auto_gl_context(this);
  GLint max_texture_size;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
  CHECK_GE(max_texture_size, kThumbnailsPageSize.width());
  CHECK_GE(max_texture_size, kThumbnailsPageSize.height());

  thumbnails_fbo_size_ = kThumbnailsPageSize;
  thumbnail_size_ = kThumbnailSize;

  glGenFramebuffersEXT(1, &thumbnails_fbo_id_);
  glGenTextures(1, &thumbnails_texture_id_);
  glBindTexture(GL_TEXTURE_2D, thumbnails_texture_id_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, thumbnails_fbo_size_.width(),
               thumbnails_fbo_size_.height(), 0, GL_RGB,
               GL_UNSIGNED_SHORT_5_6_5, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            thumbnails_texture_id_, 0);

  GLenum fb_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  CHECK(fb_status == GL_FRAMEBUFFER_COMPLETE) << fb_status;
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glBindFramebufferEXT(GL_FRAMEBUFFER,
                       gl_surface_->GetBackingFramebufferObject());

  // These vertices and texture coords map (0,0) in the texture to the bottom
  // left of the viewport. Since we get the video frames with the the top left
  // at (0,0) we need to flip the texture y coordinate in the vertex shader for
  // this to be rendered the right way up. In the case of thumbnail rendering we
  // use the same vertex shader to render the FBO to the screen, where we do not
  // want this flipping. Vertices are 2 floats for position and 2 floats for
  // texcoord each.
  const float kVertices[] = {
      -1, 1,  0, 1,  // Vertex 0
      -1, -1, 0, 0,  // Vertex 1
      1,  1,  1, 1,  // Vertex 2
      1,  -1, 1, 0,  // Vertex 3
  };
  const GLvoid* kVertexPositionOffset = 0;
  const GLvoid* kVertexTexcoordOffset =
      reinterpret_cast<GLvoid*>(sizeof(float) * 2);
  const GLsizei kVertexStride = sizeof(float) * 4;

  glGenBuffersARB(1, &vertex_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);

  program_ = glCreateProgram();
  RenderingHelper::CreateShader(program_, GL_VERTEX_SHADER, kVertexShader,
                                base::size(kVertexShader));
  RenderingHelper::CreateShader(program_, GL_FRAGMENT_SHADER, kFragmentShader,
                                base::size(kFragmentShader));
  glLinkProgram(program_);
  GLint result = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &result);
  if (!result) {
    constexpr GLsizei kLogBufferSize = 4096;
    char log[kLogBufferSize];
    glGetShaderInfoLog(program_, kLogBufferSize, NULL, log);
    LOG(FATAL) << log;
  }
  glUseProgram(program_);
  glDeleteProgram(program_);

  glUniform1i(glGetUniformLocation(program_, "tex_flip"), 0);
  glUniform1i(glGetUniformLocation(program_, "tex"), 0);
  GLint tex_external = glGetUniformLocation(program_, "tex_external");
  if (tex_external != -1) {
    glUniform1i(tex_external, 1);
  }
  GLint pos_location = glGetAttribLocation(program_, "in_pos");
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, kVertexStride,
                        kVertexPositionOffset);
  GLint tc_location = glGetAttribLocation(program_, "in_tc");
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, kVertexStride,
                        kVertexTexcoordOffset);

  // Unbind the vertex buffer
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void FrameRendererThumbnail::DestroyThumbnailImage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  AutoGLContext auto_gl_context(this);
  glDeleteTextures(1, &thumbnails_texture_id_);
  glDeleteFramebuffersEXT(1, &thumbnails_fbo_id_);
  glDeleteBuffersARB(1, &vertex_buffer_);
}

void FrameRendererThumbnail::RenderThumbnail(uint32_t texture_target,
                                             uint32_t texture_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  const int width = thumbnail_size_.width();
  const int height = thumbnail_size_.height();
  const int thumbnails_in_row = thumbnails_fbo_size_.width() / width;
  const int thumbnails_in_column = thumbnails_fbo_size_.height() / height;
  const int row = (frame_count_ / thumbnails_in_row) % thumbnails_in_column;
  const int col = frame_count_ % thumbnails_in_row;
  gfx::Rect area(col * width, row * height, width, height);

  AutoGLContext auto_gl_context(this);
  glUniform1i(glGetUniformLocation(program_, "tex_flip"), 0);
  glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
  RenderingHelper::GLSetViewPort(area);
  RenderingHelper::RenderTexture(texture_target, texture_id);
  glBindFramebufferEXT(GL_FRAMEBUFFER,
                       gl_surface_->GetBackingFramebufferObject());
  // We need to flush the GL commands before returning the thumbnail texture to
  // the decoder.
  glFlush();

  ++frame_count_;
}

const std::vector<uint8_t> FrameRendererThumbnail::ConvertThumbnailToRGBA() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  AutoGLContext auto_gl_context(this);
  std::vector<uint8_t> rgba;
  const size_t num_pixels = thumbnails_fbo_size_.GetArea();
  rgba.resize(num_pixels * 4);
  glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  // We can only count on GL_RGBA/GL_UNSIGNED_BYTE support.
  glReadPixels(0, 0, thumbnails_fbo_size_.width(),
               thumbnails_fbo_size_.height(), GL_RGBA, GL_UNSIGNED_BYTE,
               &(rgba)[0]);
  glBindFramebufferEXT(GL_FRAMEBUFFER,
                       gl_surface_->GetBackingFramebufferObject());

  return rgba;
}

void FrameRendererThumbnail::DeleteTexture(const gpu::Mailbox& mailbox,
                                           const gpu::SyncToken&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  AutoGLContext auto_gl_context(this);
  base::AutoLock auto_lock(renderer_lock_);
  auto it = mailbox_texture_map_.find(mailbox);
  ASSERT_NE(it, mailbox_texture_map_.end());
  uint32_t texture_id = it->second;
  mailbox_texture_map_.erase(mailbox);

  RenderingHelper::DeleteTexture(texture_id);
}

}  // namespace test
}  // namespace media
