#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "../alloc/alloc_chunk.h"

#include "../io/io_archive.h"

#include "gfx_device.h"

namespace as {

/**
 * \brief Transfer operation type
 *
 * Used by the internal worker to determine
 * what to do with a given transfer command.
 */
enum class GfxTransferOpType : uint32_t {
  /** No operation */
  eNone,
  /** Buffer upload */
  eUploadBuffer,
  /** Image upload */
  eUploadImage,
  /** Flush current batch */
  eFlush,
  /** Retires batch */
  eRetire,
  /** Stop worker threads */
  eStop,
};


/**
 * \brief Tansfer operation
 */
struct GfxTransferOp {
  /** Archive sub-file to read from */
  IoArchiveSubFileRef subFile;
  /** Transfer batch ID */
  uint64_t batchId = 0;
  /** Allocated staging buffer range */
  uint64_t stagingBufferOffset = 0;
  uint64_t stagingBufferSize = 0;
  /** Destination buffer for buffer uploads */
  GfxBuffer dstBuffer;
  uint64_t dstBufferOffset = 0;
  /** Scratch buffer reference for retriement */
  GfxBuffer scratchBuffer;
  uint64_t scratchBufferOffset = 0;
  uint64_t scratchBufferSize = 0;
  /** Destination image for image uploads */
  GfxImage dstImage;
  GfxImageSubresource dstImageSubresources;
  /** Transfer operation */
  GfxTransferOpType type = GfxTransferOpType::eNone;
};


/**
 * \brief Asynchronous transfer manager
 *
 * Implements transfers on top of the compute upload queue,
 * integrating with I/O archives in order to transparently
 * perform decompression as necessary.
 *
 * Internally, this will hold a large system memory staging
 * buffer, which effectively throttles transfers in case of
 * a bottleneck.
 *
 * As for the execution model, transfers will execute and
 * complete in the order they are submitted. This may in
 * some cases reduce efficiency, but makes synchronization
 * with transfers significantly more convenient since only
 * the batch ID from the last submission needs to be
 * remembered.
 *
 * All methods in this class are thread-safe, however no
 * lifetime management is performed. All objects involved
 * in a transfer operation \e must be kept alive until the
 * transfer has completed.
 */
class GfxTransferManagerIface {
  constexpr static size_t ContextCount = 4;
public:

  /**
   * \brief Initializes transfer manager
   *
   * \param [in] io I/O subsystem instance
   * \param [in] device Graphics device
   * \param [in] stagingBufferSize Size of the system memory
   *    buffer, in bytes. No single transfer operation may
   *    be larger than the staging buffer size.
   */
  GfxTransferManagerIface(
          Io                            io,
          GfxDevice                     device,
          uint64_t                      stagingBufferSize);

  /**
   * \brief Destroys transfer manager
   *
   * Waits for all pending transfers to complete.
   */
  ~GfxTransferManagerIface();

  /**
   * \brief Enqueues a buffer upload
   *
   * All buffer data in the sub-file will be copied to the
   * destination buffer at the given offset. If the source
   * file is compressed, the buffer must have been created
   * with \c GfxUsage::eDecompressionDst.
   *
   * If the buffer allows CPU write access and the source
   * data is not compressed, no staging memory will be used
   * as the buffer can be written to directly.
   * \param [in] subFile Archive sub file containing the data
   * \param [in] buffer Destination buffer
   * \param [in] offset Destination buffer offset
   * \returns Transfer batch ID for synchronization
   */
  uint64_t uploadBuffer(
          IoArchiveSubFileRef           subFile,
          GfxBuffer                     buffer,
          uint64_t                      offset);

  /**
   * \brief Enqueues a texture upload
   *
   * Only full subresource uploads can be performed, so the
   * destination image \e must be sized appropriately. When
   * uploading multiple mip levels at once, subresource data
   * must be tightly packed.
   *
   * In order to avoid queue ownership issues, the image should
   * be created with \c GfxImageFlag::eSimultaneousAccess. Only
   * a simple barrier invalidating caches will be needed on the
   * queues using the image in that case.
   * \param [in] subFile Archive sub file containing the data
   * \param [in] image Destination image
   * \param [in] subresources Destination subresources
   * \returns Transfer batch ID for synchronization
   */
  uint64_t uploadImage(
          IoArchiveSubFileRef           subFile,
          GfxImage                      image,
    const GfxImageSubresource&          subresources);

  /**
   * \brief Flushes current transfer batch
   *
   * This generally does not have to be called since polling the
   * batch ID every frame will implicitly flush, however doing so
   * may be useful if per-resource batch ID tracking is not desired.
   *
   * No operation will be performed if no transfer is queued up.
   * \returns ID of the submitted transfer batch. This is always
   *    equal to the batch ID of the last enqueued transfer operation.
   */
  uint64_t flush();

  /**
   * \brief Retrieves last completed batch ID
   *
   * This is the preferred way of synchronizing with pending
   * transfers. All resources that were uploaded with a batch
   * ID less than or equal to the last completed batch ID can
   * safely be used.
   *
   * This may flush the current batch in order to guarantee
   * forward progress.
   * \returns \c true if the batch has completed
   */
  uint64_t getCompletedBatchId();

  /**
   * \brief Waits for a given transfer batch to complete
   *
   * This should be used sparingly, e.g. when loading a minimal
   * set of resources at application startup without which the
   * application cannot run in any meaningful way, such as UI
   * textures and font resources.
   * \param [in] batch ID of the transfer batch to wait for
   */
  void waitForCompletion(
          uint64_t                      batch);

private:

  Io                                m_io;
  GfxDevice                         m_device;
  bool                              m_gpuDecompression;

  ChunkAllocator<uint64_t>          m_stagingAllocator;
  GfxBuffer                         m_stagingBuffer;
  GfxBuffer                         m_scratchBuffer;

  GfxSemaphore                      m_semaphore;

  std::mutex                        m_mutex;

  uint64_t                          m_batchId = 1;
  uint64_t                          m_batchSize = 0;

  std::array<GfxContext, ContextCount> m_contexts;

  std::condition_variable           m_retireCond;
  std::condition_variable           m_submissionCond;
  std::queue<GfxTransferOp>         m_submissionQueue;
  std::thread                       m_submissionThread;

  std::condition_variable           m_completionCond;
  std::queue<GfxTransferOp>         m_completionQueue;
  std::thread                       m_completionThread;

  uint64_t flushLocked();

  uint64_t enqueueLocked(
          GfxTransferOp&&               op);

  GfxContext acquireContext(
          uint64_t                      batchId);

  void submit();

  void retire();

  uint64_t computeAlignedSize(
    const IoArchiveSubFile&             subFile) const;

  bool useGpuDecompression(
    const IoArchiveSubFile&             subFile) const;

  bool useDirectUpload(
    const GfxTransferOp&                op) const;

};


/**
 * \brief Transfer manager object
 *
 * See GfxTransferManagerIface.
 */
class GfxTransferManager : public IfaceRef<GfxTransferManagerIface> {

public:

  GfxTransferManager() { }
  GfxTransferManager(std::nullptr_t) { }

  GfxTransferManager(
          Io                            io,
          GfxDevice                     device,
          uint64_t                      stagingBufferSize)
  : IfaceRef<GfxTransferManagerIface>(std::make_shared<GfxTransferManagerIface>(
      std::move(io), std::move(device), stagingBufferSize)) { }

};

}
