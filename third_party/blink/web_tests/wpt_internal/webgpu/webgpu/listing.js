// AUTO-GENERATED - DO NOT EDIT. See src/common/tools/gen_listings.ts.

export const listing = [
  {
    "file": [],
    "readme": "WebGPU conformance test suite."
  },
  {
    "file": [
      "api"
    ],
    "readme": "Tests for full coverage of the Javascript API surface of WebGPU."
  },
  {
    "file": [
      "api",
      "operation"
    ],
    "readme": "Tests that check the result of performing valid WebGPU operations, taking advantage of\nparameterization to exercise interactions between features."
  },
  {
    "file": [
      "api",
      "operation",
      "buffers"
    ],
    "readme": "GPUBuffer tests."
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "create_mapped"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map_detach"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map_oom"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "basic"
    ],
    "description": "Basic tests."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "compute",
      "basic"
    ],
    "description": "Basic command buffer compute tests."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "copies"
    ],
    "description": "copy{Buffer,Texture}To{Buffer,Texture} tests."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "render",
      "basic"
    ],
    "description": "Basic command buffer rendering tests."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "render",
      "rendering"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "render",
      "storeop"
    ],
    "description": "renderPass store op test that drawn quad is either stored or cleared based on storeop"
  },
  {
    "file": [
      "api",
      "operation",
      "fences"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "operation",
      "resource_init",
      "copied_texture_clear"
    ],
    "description": "Test uninitialized textures are initialized to zero when copied."
  },
  {
    "file": [
      "api",
      "operation",
      "resource_init",
      "depth_stencil_attachment_clear"
    ],
    "description": "Test uninitialized textures are initialized to zero when used as a depth/stencil attachment."
  },
  {
    "file": [
      "api",
      "operation",
      "resource_init",
      "sampled_texture_clear"
    ],
    "description": "Test uninitialized textures are initialized to zero when sampled."
  },
  {
    "file": [
      "api",
      "regression"
    ],
    "readme": "One-off tests that reproduce API bugs found in implementations to prevent the bugs from\nappearing again."
  },
  {
    "file": [
      "api",
      "validation"
    ],
    "readme": "Positive and negative tests for all the validation rules of the API."
  },
  {
    "file": [
      "api",
      "validation",
      "createBindGroup"
    ],
    "description": "createBindGroup validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "createBindGroupLayout"
    ],
    "description": "createBindGroupLayout validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "createPipelineLayout"
    ],
    "description": "createPipelineLayout validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "createRenderPipeline"
    ],
    "description": "createRenderPipeline validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "createTexture"
    ],
    "description": "createTexture validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "createView"
    ],
    "description": "createView validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "error_scope"
    ],
    "description": "error scope validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "fences"
    ],
    "description": "fences validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "queue_submit"
    ],
    "description": "queue submit validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass"
    ],
    "description": "render pass validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass_descriptor"
    ],
    "description": "render pass descriptor validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "setBindGroup"
    ],
    "description": "setBindGroup validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "setBlendColor"
    ],
    "description": "setBlendColor validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "setScissorRect"
    ],
    "description": "setScissorRect validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "setStencilReference"
    ],
    "description": "setStencilReference validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "setVertexBuffer"
    ],
    "description": "setVertexBuffer validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "setViewport"
    ],
    "description": "setViewport validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "vertex_state"
    ],
    "description": "vertexState validation tests."
  },
  {
    "file": [
      "examples"
    ],
    "description": "Examples of writing CTS tests with various features.\n\nStart here when looking for examples of basic framework usage."
  },
  {
    "file": [
      "idl"
    ],
    "readme": "Tests to check that the WebGPU IDL is correctly implemented, for examples that objects exposed\nexactly the correct members, and that methods throw when passed incomplete dictionaries."
  },
  {
    "file": [
      "shader"
    ],
    "readme": "Tests for full coverage of the shaders that can be passed to WebGPU."
  },
  {
    "file": [
      "shader",
      "execution"
    ],
    "readme": "Tests that check the result of valid shader execution."
  },
  {
    "file": [
      "shader",
      "execution",
      "robust_access"
    ],
    "description": "Tests to check array clamping in shaders is correctly implemented including vector / matrix indexing"
  },
  {
    "file": [
      "shader",
      "regression"
    ],
    "readme": "One-off tests that reproduce shader bugs found in implementations to prevent the bugs from\nappearing again."
  },
  {
    "file": [
      "shader",
      "validation"
    ],
    "readme": "Positive and negative tests for all the validation rules of the shading language."
  },
  {
    "file": [
      "web-platform"
    ],
    "readme": "Tests for Web platform-specific interactions like GPUSwapChain and canvas, WebXR,\nImageBitmaps, and video APIs."
  },
  {
    "file": [
      "web-platform",
      "canvas",
      "context_creation"
    ],
    "description": ""
  },
  {
    "file": [
      "web-platform",
      "copyImageBitmapToTexture"
    ],
    "description": "copy imageBitmap To texture tests."
  }
];
