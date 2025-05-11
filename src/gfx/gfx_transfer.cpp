#include "../util/util_log.h"

#include "gfx_transfer.h"

namespace as {

GfxTransferManagerIface::GfxTransferManagerIface(
        Io                            io,
        GfxDevice                     device,
        uint64_t                      stagingBufferSize)
: m_io                (std::move(io))
, m_device            (std::move(device))
, m_gpuDecompression  (m_device->getFeatures().gdeflateDecompression)
, m_stagingAllocator  (stagingBufferSize) {
  GfxBufferDesc bufferDesc;
  bufferDesc.debugName = "GfxTransferManager staging buffer";
  bufferDesc.usage = GfxUsage::eTransferSrc | GfxUsage::eCpuWrite | GfxUsage::eDecompressionSrc;
  bufferDesc.size = stagingBufferSize;
  bufferDesc.flags = GfxBufferFlag::eDedicatedAllocation;

  m_stagingBuffer = m_device->createBuffer(bufferDesc, GfxMemoryType::eSystemMemory);

  GfxSemaphoreDesc semaphoreDesc;
  semaphoreDesc.debugName = "GfxTransferManager semaphore";

  m_semaphore = m_device->createSemaphore(semaphoreDesc);

  for (size_t i = 0; i < ContextCount; i++)
    m_contexts[i] = m_device->createContext(GfxQueue::eComputeTransfer);

  m_submissionThread = std::thread([this] { submit(); });
  m_completionThread = std::thread([this] { retire(); });
}


GfxTransferManagerIface::~GfxTransferManagerIface() {
  std::unique_lock lock(m_mutex);

  flushLocked();

  GfxTransferOp op;
  op.type = GfxTransferOpType::eStop;

  m_submissionQueue.push(std::move(op));
  m_submissionCond.notify_one();

  lock.unlock();

  m_submissionThread.join();
  m_completionThread.join();
}


uint64_t GfxTransferManagerIface::uploadBuffer(
        IoArchiveSubFileRef           subFile,
        GfxBuffer                     buffer,
        uint64_t                      offset) {
  std::unique_lock lock(m_mutex);

  GfxTransferOp op;
  op.type = GfxTransferOpType::eUploadBuffer;
  op.subFile = std::move(subFile);
  op.dstBuffer = std::move(buffer);
  op.dstBufferOffset = offset;

  return enqueueLocked(std::move(op));
}


uint64_t GfxTransferManagerIface::uploadImage(
        IoArchiveSubFileRef           subFile,
        GfxImage                      image,
  const GfxImageSubresource&          subresources) {
  std::unique_lock lock(m_mutex);

  GfxTransferOp op;
  op.type = GfxTransferOpType::eUploadImage;
  op.subFile = std::move(subFile);
  op.dstImage = std::move(image);
  op.dstImageSubresources = subresources;

  return enqueueLocked(std::move(op));
}


uint64_t GfxTransferManagerIface::flush() {
  std::unique_lock lock(m_mutex);
  return flushLocked();
}


uint64_t GfxTransferManagerIface::getCompletedBatchId() {
  flush();

  return m_semaphore->getCurrentValue();
}


void GfxTransferManagerIface::waitForCompletion(
        uint64_t                      batch) {
  { std::unique_lock lock(m_mutex);

    if (batch >= m_batchId)
      flushLocked();
  }

  return m_semaphore->wait(batch);
}


uint64_t GfxTransferManagerIface::flushLocked() {
  if (!m_batchSize)
    return m_batchId - 1;

  GfxTransferOp op;
  op.type = GfxTransferOpType::eFlush;
  op.batchId = m_batchId;

  m_submissionQueue.push(std::move(op));
  m_submissionCond.notify_one();

  m_batchSize = 0;
  return m_batchId++;
}


uint64_t GfxTransferManagerIface::enqueueLocked(
        GfxTransferOp&&               op) {
  uint64_t alignedSize = computeAlignedSize(*op.subFile);

  // We can't allow any single batch to be larger than the
  // staging buffer, so flush early if that's a problem.
  if (m_batchSize + alignedSize > m_stagingAllocator.capacity())
    flushLocked();

  // Enqueue the operation. We do not have to wake up the
  // worker thread since it won't do anything useful until
  // the batch is flushed anyway.
  uint64_t batchId = m_batchId;
  op.batchId = batchId;

  m_submissionQueue.push(std::move(op));

  // Flush current batch if it uses at least a quarter of
  // the staging buffer. This should help reduce stalls.
  m_batchSize += alignedSize;

  if (m_batchSize >= m_stagingAllocator.capacity() / 4)
    flushLocked();

  return batchId;
}


GfxContext GfxTransferManagerIface::acquireContext(
        uint64_t                      batchId) {
  if (batchId >= ContextCount)
    m_semaphore->wait(batchId - ContextCount);

  GfxContext context = m_contexts[batchId % ContextCount];
  context->reset();

  return context;
}


void GfxTransferManagerIface::submit() {
  std::vector<GfxTransferOp> ops;

  while (true) {
    std::unique_lock lock(m_mutex);

    m_submissionCond.wait(lock, [this] {
      return !m_submissionQueue.empty();
    });

    // If the next operation in queue is an upload, just
    // add it to the local buffer, otherwise flush.
    while (!m_submissionQueue.empty()) {
      GfxTransferOp op = std::move(m_submissionQueue.front());
      m_submissionQueue.pop();

      if (op.type == GfxTransferOpType::eStop) {
        // Forward stop event to completion worker and exit
        m_completionQueue.push(std::move(op));
        m_completionCond.notify_one();
        return;
      }

      if (op.type == GfxTransferOpType::eUploadBuffer || op.type == GfxTransferOpType::eUploadImage) {
        ops.push_back(std::move(op));
        continue;
      }

      if (op.type == GfxTransferOpType::eFlush) {
        // Allocate staging memory in one go. Otherwise we'd get
        // deadlocks if the allocator is too fragmented. Do not
        // allocate staging memory for direct-upload buffers.
        uint64_t stagingBufferSize = 0;
        uint64_t stagingBufferOffset = 0;

        for (auto& op : ops) {
          if (!useDirectUpload(op)) {
            op.stagingBufferOffset = stagingBufferSize;
            op.stagingBufferSize = computeAlignedSize(*op.subFile);

            stagingBufferSize += op.stagingBufferSize;
          }
        }

        // If necessary, wait for staging memory to get freed
        m_retireCond.wait(lock, [this, stagingBufferSize, &stagingBufferOffset] {
          auto offset = m_stagingAllocator.alloc(stagingBufferSize, 1);

          if (!offset)
            return false;

          stagingBufferOffset = *offset;
          return true;
        });

        lock.unlock();

        // Build and submit the I/O request
        IoRequest request = m_io->createRequest();

        for (auto& op : ops) {
          auto archive = op.subFile.container();
          op.stagingBufferOffset += stagingBufferOffset;

          if (useDirectUpload(op)) {
            op.subFile->read(request,
              op.dstBuffer->map(GfxUsage::eCpuWrite, op.dstBufferOffset));
          } else {
            if (useGpuDecompression(*op.subFile)) {
              op.subFile->readCompressed(request,
                m_stagingBuffer->map(GfxUsage::eCpuWrite, op.stagingBufferOffset));
            } else {
              op.subFile->read(request,
                m_stagingBuffer->map(GfxUsage::eCpuWrite, op.stagingBufferOffset));
            }
          }
        }

        m_io->submit(request);

        // Figure out how large the scratch buffer for image decompression needs
        // to be, and recreate it with at least the required size if necessary.
        uint64_t scratchBufferSize = 0;

        for (auto& op : ops) {
          if (op.type == GfxTransferOpType::eUploadImage && useGpuDecompression(*op.subFile)) {
            op.scratchBufferSize = align<uint64_t>(op.subFile->getSize(), 256);
            scratchBufferSize = std::max(scratchBufferSize, op.scratchBufferSize);
          }
        }

        if (scratchBufferSize && (!m_scratchBuffer || m_scratchBuffer->getDesc().size < scratchBufferSize)) {
          GfxBufferDesc scratchDesc;
          scratchDesc.debugName = "GfxTransferManager scratch buffer";
          scratchDesc.usage = GfxUsage::eTransferSrc | GfxUsage::eDecompressionDst;
          scratchDesc.size = std::max(1ull << findmsb(scratchBufferSize - 1), 16ull << 20);
          scratchDesc.flags = GfxBufferFlag::eDedicatedAllocation;

          m_scratchBuffer = m_device->createBuffer(scratchDesc, GfxMemoryType::eAny);
        }

        // Acquire a context for command recording
        GfxContext context = acquireContext(op.batchId);

        // Start with initializing all images to allow batching barriers.
        for (const auto& op : ops) {
          if (op.type == GfxTransferOpType::eUploadImage) {
            context->imageBarrier(op.dstImage, op.dstImageSubresources,
              0, 0, GfxUsage::eTransferDst, 0, GfxBarrierFlag::eDiscard);
          }
        }

        // Record all buffer decompression and copy commands. These
        // do not use scratch memory and can work independently.
        for (const auto& op : ops) {
          if (op.type != GfxTransferOpType::eUploadBuffer || useDirectUpload(op))
            continue;

          if (useGpuDecompression(*op.subFile)) {
            context->decompressBuffer(
              op.dstBuffer, op.dstBufferOffset, op.subFile->getSize(),
              m_stagingBuffer, op.stagingBufferOffset, op.subFile->getCompressedSize());
          } else {
            context->copyBuffer(op.dstBuffer, op.dstBufferOffset,
              m_stagingBuffer, op.stagingBufferOffset, op.subFile->getSize());
          }
        }

        // Record all image decompression and copy commands, but try to
        // batch them as much as possible within the scratch buffer.
        size_t firstCommand = 0;

        while (firstCommand < ops.size()) {
          size_t commandCount = 0;

          uint64_t scratchSize = m_scratchBuffer ? m_scratchBuffer->getDesc().size : 0;
          uint64_t scratchOffset = 0;

          // If this is not the first set of commands using scratch memory,
          // issue a barrier to prevent write-after-read hazards.
          if (firstCommand) {
            context->memoryBarrier(
              GfxUsage::eTransferSrc, 0,
              GfxUsage::eDecompressionDst, 0);
          }

          while (firstCommand + commandCount < ops.size()) {
            auto& op = ops[firstCommand + commandCount];

            if (op.type != GfxTransferOpType::eUploadImage || !useGpuDecompression(*op.subFile)) {
              commandCount += 1;
              continue;
            }

            // If the scratch buffer is full, exit and record copy commands first.
            if (scratchOffset + op.scratchBufferSize > scratchSize)
              break;

            // Otherwise, record the decompression command
            op.scratchBufferOffset = scratchOffset;

            context->decompressBuffer(
              m_scratchBuffer, op.scratchBufferOffset, op.subFile->getSize(),
              m_stagingBuffer, op.stagingBufferOffset, op.subFile->getCompressedSize());

            scratchOffset += op.scratchBufferSize;
            commandCount += 1;
          }

          // If any scratch memory is used, this means that compression
          // commands have been recorded and we need a barrier.
          if (scratchOffset) {
            context->memoryBarrier(
              GfxUsage::eDecompressionDst, 0,
              GfxUsage::eTransferSrc, 0);
          }

          // Copy data from the staging or scratch buffer to the images
          for (size_t i = firstCommand; i < firstCommand + commandCount; i++) {
            const auto& op = ops[i];

            if (op.type != GfxTransferOpType::eUploadImage)
              continue;

            bool scratch = useGpuDecompression(*op.subFile);

            Extent3D extent = op.dstImage->computeMipExtent(op.dstImageSubresources.mipIndex);

            context->copyBufferToImage(op.dstImage,
              op.dstImageSubresources, Offset3D(0, 0, 0), extent,
              scratch ? m_scratchBuffer : m_stagingBuffer,
              scratch ? op.scratchBufferOffset : op.stagingBufferOffset,
              Extent2D(extent));
          }

          firstCommand += commandCount;
        }

        // Issue a final memory barrier to make transfer commands visible
        context->memoryBarrier(GfxUsage::eTransferDst | GfxUsage::eDecompressionDst, 0, 0, 0);

        // Prepare the command submission, and queue it for
        // execution when the I/O request has completed.
        request->executeOnCompletion([
          cDevice       = m_device,
          cCommandList  = context->endCommandList(),
          cSemaphore    = m_semaphore,
          cBatchId      = op.batchId
        ] (IoStatus status) mutable {
          if (status != IoStatus::eSuccess)
            Log::err("GfxTransferManager: An I/O error has occured on batch ", cBatchId);

          GfxCommandSubmission submission;
          submission.addCommandList(GfxCommandList(cCommandList));
          submission.addSignalSemaphore(cSemaphore, cBatchId);

          cDevice->submit(GfxQueue::eComputeTransfer, std::move(submission));
        });

        // Submit retire operation to the completion thread.
        lock.lock();

        GfxTransferOp retireOp;
        retireOp.type = GfxTransferOpType::eRetire;
        retireOp.batchId = op.batchId;
        retireOp.stagingBufferOffset = stagingBufferOffset;
        retireOp.stagingBufferSize = stagingBufferSize;
        retireOp.scratchBuffer = m_scratchBuffer;

        m_completionQueue.push(std::move(retireOp));
        m_completionCond.notify_one();

        ops.clear();
      }
    }
  }
}


void GfxTransferManagerIface::retire() {
  while (true) {
    std::unique_lock lock(m_mutex);

    m_completionCond.wait(lock, [this] {
      return !m_completionQueue.empty();
    });

    // Note that this operation may hold a reference to the
    // scratch buffer, which we must keep alive for now
    GfxTransferOp op = std::move(m_completionQueue.front());
    m_completionQueue.pop();

    // Exit wrker thread if requested. We can do this early
    // since stop requests have no payload attached.
    if (op.type == GfxTransferOpType::eStop)
      return;

    // We shouldn't really get anything other than retire ops
    if (op.type != GfxTransferOpType::eRetire)
      continue;

    // Unlock so that we can wait for the GPU
    lock.unlock();

    m_semaphore->wait(op.batchId);

    // Re-acquire lock and free the staging buffer region
    // attached to this this operation.
    lock.lock();

    if (op.stagingBufferSize)
      m_stagingAllocator.free(op.stagingBufferOffset, op.stagingBufferSize);

    m_retireCond.notify_one();
  }
}


uint64_t GfxTransferManagerIface::computeAlignedSize(
  const IoArchiveSubFile&             subFile) const {
  return useGpuDecompression(subFile)
    ? align<uint64_t>(subFile.getCompressedSize(), 64)
    : align<uint64_t>(subFile.getSize(), 64);
}


bool GfxTransferManagerIface::useGpuDecompression(
  const IoArchiveSubFile&             subFile) const {
  return m_gpuDecompression && subFile.getCompressionType() == IoArchiveCompression::eGDeflate;
}


bool GfxTransferManagerIface::useDirectUpload(
  const GfxTransferOp&                op) const {
  return (op.type == GfxTransferOpType::eUploadBuffer)
      && (op.dstBuffer->getDesc().usage & GfxUsage::eCpuWrite)
      && (!useGpuDecompression(*op.subFile));
}

}
