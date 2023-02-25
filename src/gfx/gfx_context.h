#pragma once

#include "../util/util_iface.h"
#include "../util/util_likely.h"

#include "gfx_buffer.h"
#include "gfx_command_list.h"
#include "gfx_descriptor_array.h"
#include "gfx_descriptor_handle.h"
#include "gfx_image.h"
#include "gfx_pipeline.h"
#include "gfx_scratch.h"
#include "gfx_render.h"
#include "gfx_types.h"

namespace as {

/**
 * \brief Device context interface
 *
 * Device contexts are heavy-weight objects that provide
 * methods to record command lists, but also come with
 * convenience features such as a linear memory allocator
 * for shader constant buffers and temporary resources.
 */
class GfxContextIface {

public:

  virtual ~GfxContextIface() { }

  /**
   * \brief Ends current command list
   *
   * And allocates a new one internally that commands can then be
   * recorded into. This implicitly resets \e all context state.
   * \returns The resulting command list
   */
  virtual GfxCommandList endCommandList() = 0;

  /**
   * \brief Resets context
   *
   * Invalidates all command lists created from the context, frees
   * scratch memory, and resets all context state. This \e must not
   * be called before all submissions that include command lists
   * created from this context have completed on the GPU.
   */
  virtual void reset() = 0;

  /**
   * \brief Inserts a debug label
   *
   * Only has an effect if the device is in debug mode.
   * \param [in] text Label text
   * \param [in] color Label color
   */
  virtual void insertDebugLabel(
    const char*                         text,
          GfxColorValue                 color) = 0;

  /**
   * \brief Begins a scoped debug label
   *
   * Only has an effect if the device is in debug mode.
   * Scoped labels must be ended with \c endDebugLabel.
   * \param [in] text Label text
   * \param [in] color Label color
   */
  virtual void beginDebugLabel(
    const char*                         text,
          GfxColorValue                 color) = 0;

  /**
   * \brief Ends a scoped debug label
   */
  virtual void endDebugLabel() = 0;

  /**
   * \brief Allocates scratch memory
   *
   * The returned slice will remain valid until \c reset is called on the
   * context, which makes this suitable for streaming data, readbacks, as
   * well as small upload.
   *
   * A suitable memory type will be chosen based on usage and CPU access:
   * - \c GfxMemoryType::eVideoMemory if no CPU access is desired,
   * - \c GfxMemoryType::eBarMemory if only CPU write access is requested
   *      and if the resource can be accessed by shaders in any way,
   * - \c GfxMemoryType::eSystemMemory otherwise.
   * \param [in] usage Desired usage of the memory slice. The returned
   *    slice will be aligned to match the requirements of this usage bit.
   * \param [in] size Number of bytes to allocate
   * \returns Allocated buffer slice
   */
  virtual GfxScratchBuffer allocScratch(
          GfxUsageFlags                 usage,
          uint64_t                      size) = 0;

  /**
   * \brief Allocates and writes scratch memory
   *
   * Convenience method that allocates a scratch buffer and immediately
   * writes data to it. See \c allocScratchData for more details.
   * \param [in] usage Scratch buffer usage for the resulting descriptor.
   *    The necessary CPU access bits are implied and need not be set.
   * \param [in] data Data to write to the allocated slice
   * \returns Descriptor for allocated buffer slice
   */
  template<typename T>
  GfxDescriptor writeScratch(
          GfxUsage                      usage,
    const T&                            data) {
    GfxScratchBuffer slice = allocScratch(usage | GfxUsage::eCpuWrite, sizeof(data));
    std::memcpy(slice.buffer->map(GfxUsage::eCpuWrite, slice.offset), &data, sizeof(data));
    return slice.getDescriptor(usage);
  }

  /**
   * \brief Begins render pass
   *
   * Binds the given set of render targets, and clears any render
   * target for which an operation of \c GfxRenderTargetOp::eClear
   * is specified.
   *
   * The \c flags parameter can be used when recording multiple command
   * lists for a single render pass. If \c GfxRenderingFlag::eSuspend
   * is set, this \e must be the last command in the command list,
   * and the next command list submitted to the same queue \e must
   * begin with a call to \c beginRendering binding the same set of
   * render targets as well as \c GfxRenderingFlag::eResume set.
   * Additionally, if \c GfxRenderingFlag::eResume is set, the render
   * target ops will be ignored, i.e. no clears will be performed.
   *
   * Note that using the flags parameter is not required, but doing
   * so may improve performance.
   *
   * Inside a render pass instance, only draw operations and calls
   * that modify graphics pipeline state are allowed. This includes
   * allocaing temporary buffer resources and writing to them from
   * the CPU.
   * \param [in] renderingInfo Render target info
   * \param [in] flags Rendering flags
   */
  virtual void beginRendering(
    const GfxRenderingInfo&             renderingInfo,
          GfxRenderingFlags             flags) = 0;

  /**
   * \brief Ends render pass
   *
   * Ends the current render pass. Calling draw operations
   * outside of a render pass is not allowed, but all other
   * operations can be used.
   */
  virtual void endRendering() = 0;

  /**
   * \brief Emits a global memory barrier
   *
   * All subsequent commands that perform any accesses compatible with
   * \c dstUsage will be stalled until all prior accesses compatible
   * with \c srcUsage have completed. This can be used to synchronize
   * buffer access, as well as consecutive read-write accesses to images.
   * It will \e not perform any image transitions.
   *
   * If the \c srcUsage or \c dstUsage parameters do not contain any of
   * \c GfxUsage::eConstantBuffer, \c GfxUsage::eShaderResource or
   * \c GfxUsage::eShaderStorage, the corresponding stage masks have
   * no meaning and should be set to 0.
   * \param [in] srcUsage Source commands to wait for
   * \param [in] srcStages Shader stages used in source commands
   * \param [in] dstUsage Destination commands to stall
   * \param [in] dstStages Shader stages used in destination commands
   */
  virtual void memoryBarrier(
          GfxUsageFlags                 srcUsage,
          GfxShaderStages               srcStages,
          GfxUsageFlags                 dstUsage,
          GfxShaderStages               dstStages) = 0;

  /**
   * \brief Emits an image memory barrier
   *
   * Synchronizes commands as a call to \c memoryBarrier would, and
   * transitions the internal image layout in such a way that the image
   * will be compatible with all accesses specified in \c dstUsage.
   *
   * If \c flags contains \c GfxBarrierFlag::eDiscard, the image
   * will be re-initialized and all previous contents will be discarded.
   * Images \e must be initialized in this way on first use, and \e may
   * be re-initialized any time its contents are no longer needed.
   *
   * When re-initializing an image, \c srcUsage \e may be 0, however
   * beware of potential write-after-write or write-after-read hazard.
   * Any image transition is consiered to be a write.
   *
   * When an image is initialized, it will only be compatible with the
   * queue that the context was created for. In order to use an image
   * that was created without \c GfxImageFlag::eSimultaneousAccess on
   * different queues, use \c releaseImage and \c acquireImage.
   *
   * In general, \c dstUsage must only have one bit set. The only
   * exception is for depth-stencil images that are to be bound for
   * rendering as well as a shader resource, in which case both the
   * \c GfxUsage::eRenderTarget and \c GfxUsage::eShaderResource
   * flags can be set.
   *
   * If \c srcUsage and \c dstUsage are identical and \c flags is 0,
   * this command behaves the same as a call to \c memoryBarrier.
   *
   * \note Overlapping access to images on different queues \e must
   *    be synchronized properly with semaphores, even if the image
   *    was created with \c GfxImageFlag::eSimultaneousAccess, unless
   *    all accesses are reads.
   * \param [in] image Image to transition
   * \param [in] subresource Subresources to transition
   * \param [in] srcUsage Previous image usage. Must be identical to
   *    the \c dstUsage parameter used in a prior image barrier, or
   * \param [in] srcStages Shader stages used in source commands
   * \param [in] dstUsage New image usage. The image will only be
   *    compatible with the given type of accesses, and must be
   *    transitioned again before using it in a different way.
   * \param [in] dstStages Shader stages used in destination commands
   * \param [in] flags Image transition flags
   */
  virtual void imageBarrier(
    const GfxImage&                     image,
    const GfxImageSubresource&          subresource,
          GfxUsageFlags                 srcUsage,
          GfxShaderStages               srcStages,
          GfxUsageFlags                 dstUsage,
          GfxShaderStages               dstStages,
          GfxBarrierFlags               flags) = 0;

  /**
   * \brief Acquires image from another queue
   *
   * The \c srcUsage, \c dstUsage and \c dstStages parameters behave
   * the same as they do in a call to \c imageBarrier.
   * \param [in] image Image to acquire
   * \param [in] subresource Subresources to acquire
   * \param [in] srcQueue Queue that the image was released from
   * \param [in] srcUsage Last image usage on the source queue
   * \param [in] dstUsage Desired image usage and destination commands
   * \param [in] dstStages Shader stages used in destination commands
   */
  virtual void acquireImage(
    const GfxImage&                     image,
    const GfxImageSubresource&          subresource,
          GfxQueue                      srcQueue,
          GfxUsageFlags                 srcUsage,
          GfxUsageFlags                 dstUsage,
          GfxShaderStages               dstStages) = 0;

  /**
   * \brief Releases image so it can be used on another queue
   *
   * The \c srcUsage, \c srcStages and \c dstUsage parameters behave
   * the same as they do in a call to \c imageBarrier.
   * \param [in] image Image to acquire
   * \param [in] subresource Subresources to acquire
   * \param [in] srcUsage Current image usage and source commands
   * \param [in] srcStages Shader stages used in source commands
   * \param [in] dstQueue Queue that the image will be used on
   * \param [in] dstUsage Desired image usage on destination queue
   */
  virtual void releaseImage(
    const GfxImage&                     image,
    const GfxImageSubresource&          subresource,
          GfxUsageFlags                 srcUsage,
          GfxShaderStages               srcStages,
          GfxQueue                      dstQueue,
          GfxUsageFlags                 dstUsage) = 0;

  /**
   * \brief Binds compute pipeline to the context
   *
   * If the pipeline is not available, the calling thread may
   * be stalled until background compilation has completed.
   * \param [in] pipeline Pipeline to bind
   */
  virtual void bindPipeline(
          GfxComputePipeline            pipeline) = 0;

  /**
   * \brief Binds graphics pipeline to the context
   *
   * If the pipeline is not available, the calling thread may
   * be stalled until background compilation has completed.
   * \param [in] pipeline Pipeline to bind
   */
  virtual void bindPipeline(
          GfxGraphicsPipeline           pipeline) = 0;

  /**
   * \brief Binds descriptor array to a given set
   *
   * This will invalidate all descriptors
   * previously bound to the same set.
   * \param [in] set Descriptor set index
   * \param [in] array Descriptor array
   */
  virtual void bindDescriptorArray(
          uint32_t                      set,
    const GfxDescriptorArray&           array) = 0;

  /**
   * \brief Sets descriptors for a given set
   *
   * This will invalidate any descriptor array
   * previously bound to the same set.
   * \param [in] set Descriptor set index
   * \param [in] index First descriptor to set
   * \param [in] count Number of descriptors to set
   * \param [in] descriptors Descriptors
   */
  virtual void bindDescriptors(
          uint32_t                      set,
          uint32_t                      index,
          uint32_t                      count,
    const GfxDescriptor*                descriptors) = 0;

  /**
   * \brief Sets a single descriptor for a given set
   *
   * Convenience method that should only be used if
   * batching descriptor updates is not useful.
   * \param [in] set Descriptor set index
   * \param [in] index Index of descriptor to set
   * \param [in] descriptor Descriptor
   */
  void bindDescriptor(
          uint32_t                      set,
          uint32_t                      index,
    const GfxDescriptor&                descriptor) {
    return bindDescriptors(set, index, 1, &descriptor);
  }

  /**
   * \brief Sets index buffer
   *
   * Indexed draws \e must have a valid index buffer bound.
   * The index buffer format \e must be one of:
   * - \c GfxFormat::eR16ui
   * - \c GfxFormat::eR32ui
   * \param [in] descriptor Index buffer descriptor
   * \param [in] format Index format
   */
  virtual void bindIndexBuffer(
    const GfxDescriptor&                descriptor,
          GfxFormat                     format) = 0;

  /**
   * \brief Sets vertex buffers
   *
   * Descriptors may be null, in which case vertex data
   * read from the given binding will read all zero.
   * \param [in] index First vertex buffer to set
   * \param [in] count Number of vertex buffers to set
   * \param [in] descriptors Vertex buffer descriptors
   * \param [in] strides Vertex buffer strides
   */
  virtual void bindVertexBuffers(
          uint32_t                      index,
          uint32_t                      count,
    const GfxDescriptor*                descriptors,
    const uint32_t*                     strides) = 0;

  /**
   * \brief Sets a single vertex buffer
   *
   * Convenience method that should only be used if
   * batching vertex buffer updates is not useful.
   * \param [in] index Vertex buffer to set
   * \param [in] descriptor Vertex buffer descriptor
   * \param [in] stride Vertex buffer stride
   */
  void bindVertexBuffer(
          uint32_t                      index,
    const GfxDescriptor&                descriptor,
          uint32_t                      stride) {
    bindVertexBuffers(index, 1, &descriptor, &stride);
  }

  /**
   * \brief Copies buffer data
   *
   * \param [in] dstBuffer Buffer to write to
   * \param [in] dstOffset Destination offset, in bytes
   * \param [in] srcBuffer Buffer to read from
   * \param [in] srcOffset Source offset, in bytes
   * \param [in] size Number of bytes to copy
   */
  virtual void copyBuffer(
    const GfxBuffer&                    dstBuffer,
          uint64_t                      dstOffset,
    const GfxBuffer&                    srcBuffer,
          uint64_t                      srcOffset,
          uint64_t                      size) = 0;

  /**
   * \brief Copies buffer data to an image
   *
   * Copies packed buffer data to one or more subresources
   * of the given image.
   *
   * The \c bufferLayout parameter describes how each 2D
   * slice or array layer is laid out in buffer memory.
   * Its components are given in texels.
   *
   * When uploading multiple mip levels or aspects, the
   * \c bufferLayout will also be scaled down accordingly.
   * No additional padding is expected between subresources.
   * \param [in] image Image to copy to
   * \param [in] imageSubresource Image subresource.
   *    Must only contain one aspect and mip level.
   * \param [in] imageOffset Offset of the image area
   * \param [in] imageExtent Size of the area to copy
   * \param [in] buffer Buffer to read from
   * \param [in] bufferOffset Buffer offset, in bytes
   * \param [in] bufferLayout Extent of the image data
   *    within the buffer, in pixels.
   */
  virtual void copyBufferToImage(
    const GfxImage&                     image,
    const GfxImageSubresource&          imageSubresource,
          Offset3D                      imageOffset,
          Extent3D                      imageExtent,
    const GfxBuffer&                    buffer,
          uint64_t                      bufferOffset,
          Extent2D                      bufferLayout) = 0;

  /**
   * \brief Copies image data
   *
   * \param [in] dstImage Image to write to
   * \param [in] dstSubresource Destination subresources
   * \param [in] dstOffset Destination image offset
   * \param [in] srcImage Image to read from
   * \param [in] srcSubresource Source subresources
   * \param [in] srcOffset Source image offset
   * \param [in] extent Extent of the area to copy
   */
  virtual void copyImage(
    const GfxImage&                     dstImage,
    const GfxImageSubresource&          dstSubresource,
          Offset3D                      dstOffset,
    const GfxImage&                     srcImage,
    const GfxImageSubresource&          srcSubresource,
          Offset3D                      srcOffset,
          Extent3D                      extent) = 0;

  /**
   * \brief Copies image data to a buffer
   *
   * The data layout matches that of \c copyBufferToImage.
   * \param [in] buffer Buffer to write to
   * \param [in] bufferOffset Buffer offset, in bytes
   * \param [in] bufferLayout Extent of the image data
   *    within the buffer, in pixels.
   * \param [in] image Image to copy from
   * \param [in] imageSubresource Image subresource.
   *    Must only contain one aspect and mip level.
   * \param [in] imageOffset Offset of the image area
   * \param [in] imageExtent Size of the area to copy
   */
  virtual void copyImageToBuffer(
    const GfxBuffer&                    buffer,
          uint64_t                      bufferOffset,
          Extent2D                      bufferLayout,
    const GfxImage&                     image,
    const GfxImageSubresource&          imageSubresource,
          Offset3D                      imageOffset,
          Extent3D                      imageExtent) = 0;

  /**
   * \brief Decompresses a buffer
   *
   * Decodes a GDeflate-encoded buffer. The buffer \e must
   * begin with a \c GDeflateHeader struct, immediately
   * followed by a packed \c GDeflatePage array.
   * 
   * Decompression commands must be synchronized using
   * the corresponding GPU decompression usage flags.
   * \param [in] dstBuffer Output buffer
   * \param [in] dstOffset Output buffer offset
   * \param [in] dstSize Decompressed data size
   * \param [in] srcBuffer Compressed buffer
   * \param [in] srcOffset Compressed buffer offset
   * \param [in] srcSize Compressed data size
   */
  virtual void decompressBuffer(
    const GfxBuffer&                    dstBuffer,
          uint64_t                      dstOffset,
          uint64_t                      dstSize,
    const GfxBuffer&                    srcBuffer,
          uint64_t                      srcOffset,
          uint64_t                      srcSize) = 0;

  /**
   * \brief Executes a compute dispatch
   *
   * Only valid outside of render passes.
   * \param workgroupCount Workgroup count vector
   */
  virtual void dispatch(
          Extent3D                      workgroupCount) = 0;

  /**
   * \brief Executes an indirect compute dispatch
   *
   * Only valid outside of render passes.
   * \param [in] args Argument buffer descriptor pointing
   *    to a single \c GfxDispatchArgs struct.
   */
  virtual void dispatchIndirect(
    const GfxDescriptor&                args) = 0;

  /**
   * \brief Executes a non-indexed draw
   *
   * \param [in] vertexCount Number of vertices
   * \param [in] instanceCount Number of instances
   * \param [in] firstVertex First vertex index
   * \param [in] firstInstance First instance index
   */
  virtual void draw(
          uint32_t                      vertexCount,
          uint32_t                      instanceCount,
          uint32_t                      firstVertex,
          uint32_t                      firstInstance) = 0;

  /**
   * \brief Executes an indirect draw
   *
   * \param [in] args Argument buffer descriptor pointing
   *    to tightly packed \c GfxDrawArgs structs.
   * \param [in] count Count buffer descriptor. If
   *    null, \c maxCount draws will be performed.
   * \param [in] maxCount Maximum number of draws
   */
  virtual void drawIndirect(
    const GfxDescriptor&                args,
    const GfxDescriptor&                count,
          uint32_t                      maxCount) = 0;

  /**
   * \brief Executes an indirect draw
   *
   * Convenience method that omits the count buffer.
   * \param [in] args Argument buffer descriptor
   * \param [in] count Number of draws to perform
   */
  void drawIndirect(
    const GfxDescriptor&                args,
          uint32_t                      count) {
    drawIndirect(args, GfxDescriptor(), count);
  }

  /**
   * \brief Executes an indexed draw
   *
   * \param [in] indexCount Number of indices
   * \param [in] instanceCount Number of instances
   * \param [in] firstIndex Offset into index buffer, in index elements
   * \param [in] firstVertex Vertex index to add to index buffer values
   * \param [in] firstInstance First instance index
   */
  virtual void drawIndexed(
          uint32_t                      indexCount,
          uint32_t                      instanceCount,
          uint32_t                      firstIndex,
          int32_t                       firstVertex,
          uint32_t                      firstInstance) = 0;

  /**
   * \brief Executes an indirect indexed draw
   *
   * \param [in] args Argument buffer descriptor pointing
   *    to tightly packed \c GfxDrawIndexedArgs structs.
   * \param [in] count Count buffer descriptor. If
   *    null, \c maxCount draws will be performed.
   * \param [in] maxCount Maximum number of draws
   */
  virtual void drawIndexedIndirect(
    const GfxDescriptor&                args,
    const GfxDescriptor&                count,
          uint32_t                      maxCount) = 0;

  /**
   * \brief Executes an indirect indexed draw
   *
   * Convenience method that omits the count buffer.
   * \param [in] args Argument buffer descriptor
   * \param [in] count Number of draws to perform
   */
  void drawIndexedIndirect(
    const GfxDescriptor&                args,
          uint32_t                      count) {
    drawIndexedIndirect(args, GfxDescriptor(), count);
  }

  /**
   * \brief Executes a mesh shader draw
   *
   * Requires a mesh shader pipeline to be bound.
   * \param workgroupCount Workgroup count vector
   */
  virtual void drawMesh(
          Extent3D                      workgroupCount) = 0;

  /**
   * \brief Executes an indirect mesh shader draw
   *
   * \param [in] args Argument buffer descriptor pointing
   *    to tightly packed \c GfxDispatchArgs structs.
   * \param [in] count Count buffer descriptor. If
   *    null, \c maxCount draws will be performed.
   * \param [in] maxCount Maximum number of draws
   */
  virtual void drawMeshIndirect(
    const GfxDescriptor&                args,
    const GfxDescriptor&                count,
          uint32_t                      maxCount) = 0;

  /**
   * \brief Executes an indirect mesh shader draw
   *
   * Convenience method that omits the count buffer.
   * \param [in] args Argument buffer descriptor
   * \param [in] count Number of draws to perform
   */
  void drawMeshIndirect(
    const GfxDescriptor&                args,
          uint32_t                      count) {
    drawMeshIndirect(args, GfxDescriptor(), count);
  }

  /**
   * \brief Sets blend constants
   *
   * Provides values for constant blend factors
   * if used in the active blend state.
   * \param [in] constants Blend constants
   */
  virtual void setBlendConstants(
          GfxColorValue                 constants) = 0;

  /**
   * \brief Sets blend state
   * \param [in] state Blend state object
   */
  virtual void setColorBlendState(
          GfxColorBlendState            state) = 0;

  /**
   * \brief Sets depth bounds
   *
   * Sets the minimum and maximum depth values to compare the
   * current depth buffer values against if the depth-stencil
   * state enables the depth bounds test.
   * \param [in] minDepth Minimum depth value
   * \param [in] maxDepth Maximum depth value
   */
  virtual void setDepthBounds(
          float                         minDepth,
          float                         maxDepth) = 0;

  /**
   * \brief Sets depth-stencil state
   * \param [in] state Depth-stencil state object
   */
  virtual void setDepthStencilState(
          GfxDepthStencilState          state) = 0;

  /**
   * \brief Sets multisample state
   * \param [in] state Multisample state object
   */
  virtual void setMultisampleState(
          GfxMultisampleState           state) = 0;

  /**
   * \brief Sets rasterizer state
   * \param [in] state Rasterizer state object
   */
  virtual void setRasterizerState(
          GfxRasterizerState            state) = 0;

  /**
   * \brief Sets shader constants
   *
   * The sum of \c offset and \c size \e must
   * not be greater than 256.
   * \param [in] offset Offset into constant block
   * \param [in] size Number of bytes to set
   * \param [in] data Constant data
   */
  virtual void setShaderConstants(
          uint32_t                      offset,
          uint32_t                      size,
    const void*                         data) = 0;

  /**
   * \brief Sets shader constants
   *
   * Convenience method to pass an arbitrary data
   * structure as a shader parameter.
   * \param [in] offset Offset into constant block
   * \param [in] data Constant data
   */
  template<typename T>
  void setShaderConstants(
          uint32_t                      offset,
    const T&                            data) {
    setShaderConstants(offset, sizeof(data), &data);
  }

  /**
   * \brief Sets stencil reference
   *
   * Sets the reference value to use for the stencil test.
   * \param [in] front Front face stencil reference
   * \param [in] back Back face stencil reference
   */
  virtual void setStencilReference(
          uint32_t                      front,
          uint32_t                      back) = 0;

  /**
   * \brief Sets vertex input state
   * \param [in] state Vertex input state object
   */
  virtual void setVertexInputState(
          GfxVertexInputState           state) = 0;

  /**
   * \brief Sets viewports
   *
   * \param [in] count Number of viewports to set
   * \param [in] viewports Viewport array
   */
  virtual void setViewports(
          uint32_t                      count,
    const GfxViewport*                  viewports) = 0;

  /**
   * \brief Sets a single viewport
   *
   * Convenience method that only enables one viewport.
   * \param [in] viewport Viewport to set
   */
  void setViewport(
    const GfxViewport&                  viewport) {
    setViewports(1, &viewport);
  }

};

/** See GfxContextIface. */
using GfxContext = IfaceRef<GfxContextIface>;

}
