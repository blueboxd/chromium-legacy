/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/

export const description = `
computePass test that sampled texture is cleared`;
import { TestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';
export const g = new TestGroup(GPUTest);
g.test('compute pass test that sampled texture is cleared', async t => {
  const texture = t.device.createTexture({
    size: {
      width: 256,
      height: 256,
      depth: 1
    },
    format: 'r8unorm',
    usage: GPUTextureUsage.SAMPLED
  });
  const bufferTex = t.device.createBuffer({
    size: 4 * 256 * 256,
    usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });
  const sampler = t.device.createSampler();
  const bindGroupLayout = t.device.createBindGroupLayout({
    entries: [{
      binding: 0,
      visibility: GPUShaderStage.COMPUTE,
      type: 'sampled-texture'
    }, {
      binding: 1,
      visibility: GPUShaderStage.COMPUTE,
      type: 'storage-buffer'
    }, {
      binding: 2,
      visibility: GPUShaderStage.COMPUTE,
      type: 'sampler'
    }]
  }); // create compute pipeline

  const computeModule = t.makeShaderModule('compute', {
    glsl: `
      #version 450
      layout(binding = 0) uniform texture2D sampleTex;
      layout(std430, binding = 1) buffer BufferTex {
         vec4 result;
      } bufferTex;
      layout(binding = 2) uniform sampler sampler0;
      void main() {
         bufferTex.result =
               texelFetch(sampler2D(sampleTex, sampler0), ivec2(0,0), 0);
      }
    `
  });
  const pipelineLayout = t.device.createPipelineLayout({
    bindGroupLayouts: [bindGroupLayout]
  });
  const computePipeline = t.device.createComputePipeline({
    computeStage: {
      module: computeModule,
      entryPoint: 'main'
    },
    layout: pipelineLayout
  }); // create bindgroup

  const bindGroup = t.device.createBindGroup({
    layout: bindGroupLayout,
    entries: [{
      binding: 0,
      resource: texture.createView()
    }, {
      binding: 1,
      resource: {
        buffer: bufferTex,
        offset: 0,
        size: 4 * 256 * 256
      }
    }, {
      binding: 2,
      resource: sampler
    }]
  }); // encode the pass and submit

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(computePipeline);
  pass.setBindGroup(0, bindGroup);
  pass.dispatch(256, 256, 1);
  pass.endPass();
  const commands = encoder.finish();
  t.device.defaultQueue.submit([commands]);
  await t.expectContents(bufferTex, new Uint32Array([0]));
});
//# sourceMappingURL=sampled_texture_clear.spec.js.map