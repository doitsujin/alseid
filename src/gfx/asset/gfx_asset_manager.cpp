#include "gfx_asset_manager.h"

namespace as {

GfxAssetManager::GfxAssetManager(
      GfxDevice                       device)
: m_device          (std::move(device))
, m_pipelines       (m_device)
, m_samplerPool     (m_device, "Sampler pool", GfxShaderBindingType::eSampler, SamplerDescriptorCount)
, m_texturePool     (m_device, "Texture pool", GfxShaderBindingType::eResourceImageView, TextureDescriptorCount)
, m_groupBuffers    (m_device, getGroupBufferDesc(), GfxMemoryType::eAny)
, m_requestWorker   ([this] { runRequestWorker(); })
, m_residencyWorker ([this] { runResidencyWorker(); }) {

}


GfxAssetManager::~GfxAssetManager() {
  GfxAssetRequest request = { };
  request.type = GfxAssetRequestType::eStopWorker;

  enqueueRequest(request);
  m_requestWorker.join();

  // Invalid asset handle to stop the residency worker
  enqueueStreamAsset(GfxAsset());
  m_residencyWorker.join();
}


void GfxAssetManager::bindDescriptorArrays(
  const GfxContext&                   context,
        uint32_t                      samplerIndex,
        uint32_t                      textureIndex) const {
  context->bindDescriptorArray(samplerIndex, m_samplerPool.descriptorArray);
  context->bindDescriptorArray(textureIndex, m_texturePool.descriptorArray);
}


GfxAsset GfxAssetManager::findAsset(
  const GfxSemanticName&              name) {
  std::unique_lock lock(m_assetLutMutex);

  auto entry = m_assetLut.find(name);

  if (entry == m_assetLut.end())
    return GfxAsset();

  return entry->second;
}


GfxAssetGroup GfxAssetManager::createAssetGroup(
  const GfxSemanticName&              name,
        GfxAssetGroupType             type,
        uint32_t                      assetCount,
  const GfxAsset*                     assets) {
  uint32_t groupIndex = m_groups.allocator.allocate();

  // Assign asset infos and compute the buffer layout right away
  auto& groupInfo = m_groups.map.emplace(groupIndex);
  groupInfo.assets.resize(assetCount);

  uint32_t dwordCount = 0u;

  // Also register asset group with each asset
  { std::unique_lock lock(m_assetMutex);

    for (uint32_t i = 0; i < assetCount; i++) {
      auto& assetInfo = getAsset(assets[i]);

      groupInfo.assets[i].asset = assets[i];
      groupInfo.assets[i].type = gfxGetAssetRefType(assetInfo.iface->getAssetInfo().type);
      groupInfo.assets[i].index = uint24_t(dwordCount);

      dwordCount += gfxGetAssetRefSize(groupInfo.assets[i].type) / sizeof(uint32_t);

      m_groupList.insert({ assets[i], GfxAssetGroup(groupIndex) });
    }

    m_dirtyGroups.push_back(GfxAssetGroup(groupIndex));
  }

  // Pad buffer size by one dword in order to allow shaders to unconditionally
  // read 8 bytes at once. This will rarely affect the overall size since we
  // pad the allocation anyway in order to avoid false data sharing.
  uint64_t dataSize = sizeof(GfxAssetListHeader) + sizeof(uint32_t) * (dwordCount + 1u);
  groupInfo.type = type;
  groupInfo.bufferSlice = m_groupBuffers.alloc(dataSize, 256ull);
  groupInfo.dwordCount = dwordCount;

  return registerNamedGroup(name, groupIndex);
}


GfxAssetGroup GfxAssetManager::findAssetGroup(
  const GfxSemanticName&              name) {
  std::unique_lock lock(m_groupLutMutex);

  auto entry = m_groupLut.find(name);

  if (entry == m_groupLut.end())
    return GfxAssetGroup();

  return entry->second;
}


void GfxAssetManager::streamAssetGroup(
        GfxAssetGroup                 group) {
  GfxAssetRequest request = { };
  request.type = GfxAssetRequestType::eRequestStream;
  request.assetGroup = group;

  enqueueRequest(request);
}


void GfxAssetManager::evictAssetGroup(
        GfxAssetGroup                 group) {
  GfxAssetRequest request = { };
  request.type = GfxAssetRequestType::eRequestEvict;
  request.assetGroup = group;

  enqueueRequest(request);
}


void GfxAssetManager::commitUpdates(
  const GfxContext&                   context,
        uint32_t                      currFrameId,
        uint32_t                      lastFrameId) {
  // Ensure that no other thread messes around with asset residency
  // while we're committing pending changes to any asset list. This is
  // especially important in order to make the new frame IDs visible.
  std::unique_lock lock(m_assetMutex);
  m_currFrameId = currFrameId;
  m_lastFrameId = lastFrameId;

  context->beginDebugLabel("Clear asset feedback buffer", 0xffffb0e3);
  uint64_t feedbackSize = computeFeedbackBufferSize();

  if (!m_feedbackBuffer || m_feedbackBuffer->getDesc().size < feedbackSize)
    m_feedbackBuffer = createFeedbackBuffer(align(feedbackSize, uint64_t(1u << 16)));

  context->clearBuffer(m_feedbackBuffer, 0, feedbackSize);
  context->endDebugLabel();

  context->beginDebugLabel("Update asset lists", 0xffffb0e3);

  for (auto group : m_dirtyGroups) {
    auto& groupInfo = m_groups.map[uint32_t(group)];

    // Ensure that we allocate enough scratch memory. We can assume that asset
    // data is tightly packed, so we only need to pad the buffer with zeroes.
    GfxScratchBuffer scratch = context->allocScratch(
      GfxUsage::eCpuWrite | GfxUsage::eShaderResource,
      align(groupInfo.dwordCount * sizeof(uint32_t), CacheLineSize));

    uint32_t* dwords = reinterpret_cast<uint32_t*>(scratch.map(GfxUsage::eCpuWrite, 0));

    bool resident = true;

    for (const auto& entry : groupInfo.assets)
      resident &= writeAssetData(&dwords[uint32_t(entry.index)], entry.type, entry.asset);

    for (uint32_t i = groupInfo.dwordCount; i < scratch.size / sizeof(uint32_t); i++)
      dwords[i] = 0u;

    // Update asset buffer. If this is the first time we write to the
    // asset list, let the shader clear everything to zero.
    GfxAssetListUpdateArgs args = { };
    args.dstAssetListVa = groupInfo.bufferSlice.getGpuAddress();
    args.srcAssetDataVa = scratch.getGpuAddress();
    args.dstDwordCount = groupInfo.dwordCount;
    args.srcDwordCount = groupInfo.dwordCount;
    args.handle = uint32_t(group);
    args.frameId = currFrameId;

    if (!groupInfo.lastCommitFrameId)
      args.dstDwordCount = (groupInfo.bufferSlice.size - sizeof(GfxAssetListHeader)) / sizeof(uint32_t);

    m_pipelines.updateAssetList(context, args, groupInfo.lastCommitFrameId == 0u);

    groupInfo.status.set(GfxAssetGroupStatus::eResident, resident);
    groupInfo.lastCommitFrameId = currFrameId;
  }

  context->endDebugLabel();

  m_dirtyGroups.clear();

  GfxAssetRequest request = { };
  request.type = GfxAssetRequestType::eEvictUnused;

  enqueueRequest(request);
}


GfxScratchBuffer GfxAssetManager::getFeedbackData(
  const GfxContext&                   context) {
  uint64_t feedbackSize = computeFeedbackBufferSize();

  GfxScratchBuffer scratch = context->allocScratch(
    GfxUsage::eCpuRead | GfxUsage::eTransferDst, feedbackSize);

  context->copyBuffer(scratch.buffer, scratch.offset,
    m_feedbackBuffer, 0, feedbackSize);
  return scratch;
}


void GfxAssetManager::processFeedback(
  const GfxScratchBuffer&             feedback,
        uint32_t                      frameId) {
  std::unique_lock lock(m_assetMutex);

  auto data = reinterpret_cast<const uint32_t*>(
    feedback.map(GfxUsage::eCpuRead, 0));

  // The first dword contains the number of entries in the
  // feedback buffer, the remaining dwords are group handles.
  uint32_t count = std::min(data[0], m_groups.allocator.getCount());

  for (uint32_t i = 1; i <= count; i++) {
    auto& groupInfo = m_groups.map[data[i]];

    // If the asset group was not used in the previous frame,
    // mark it as used and send a stream request.
    if ((groupInfo.type == GfxAssetGroupType::eGpuManaged)
     && (!groupInfo.lastUseFrameId || groupInfo.lastUseFrameId < m_feedbackFrameId))
      streamAssetGroup(GfxAssetGroup(data[i]));

    groupInfo.lastUseFrameId = frameId;
  }

  // Iterate over previous feedback array and mark any asset group
  // that has not been updated in the current frame as unused
  for (auto group : m_feedbackGroups) {
    auto& groupInfo = getAssetGroup(group);

    if ((groupInfo.type == GfxAssetGroupType::eGpuManaged)
     && (groupInfo.lastUseFrameId < frameId))
      evictAssetGroup(group);
  }

  // Copy feedback buffer to local feedback array for the next frame
  m_feedbackFrameId = frameId;
  m_feedbackGroups.resize(count);

  for (uint32_t i = 1; i <= count; i++)
    m_feedbackGroups[i - 1] = GfxAssetGroup(data[i]);

  feedback.unmap(GfxUsage::eCpuRead);
}


GfxAssetDescriptorPool* GfxAssetManager::getDescriptorPool(
        GfxAssetType                  type) {
  switch (type) {
    case GfxAssetType::eSampler:
      return &m_samplerPool;
    case GfxAssetType::eTexture:
      return &m_texturePool;
    default:
      return nullptr;
  }
}


uint32_t GfxAssetManager::createDescriptor(
        GfxAssetType                  type,
  const GfxDescriptor&                descriptor) {
  auto pool = getDescriptorPool(type);
  auto result = pool->allocator.alloc(m_lastFrameId);

  if (!result)
    return 0;

  uint32_t index = *result;
  pool->descriptorArray->setDescriptor(index, descriptor);
  return index;
}


void GfxAssetManager::freeDescriptor(
        GfxAssetType                  type,
        uint32_t                      index) {
  getDescriptorPool(type)->allocator.free(index, m_currFrameId);
}


GfxAssetInfo& GfxAssetManager::getAsset(
        GfxAsset                      asset) {
  return m_assets.map[uint32_t(asset)];
}


GfxAssetGroupInfo& GfxAssetManager::getAssetGroup(
        GfxAssetGroup                 assetGroup) {
  return m_groups.map[uint32_t(assetGroup)];
}


GfxAsset GfxAssetManager::createAssetWithIface(
  const GfxSemanticName&              name,
        std::unique_ptr<GfxAssetIface>&& iface) {
  uint32_t assetIndex = m_assets.allocator.allocate();

  auto& assetInfo = m_assets.map.emplace(assetIndex);
  assetInfo.iface = std::move(iface);

  return registerNamedAsset(name, assetIndex);
}


void GfxAssetManager::dirtyAssetGroups(
        GfxAsset                      asset,
        uint32_t                      currFrameId) {
  auto list = m_groupList.equal_range(asset);

  for (auto i = list.first; i != list.second; i++) {
    auto& group = m_groups.map[uint32_t(i->second)];

    if (currFrameId > group.lastUpdateFrameId) {
      group.lastUpdateFrameId = currFrameId;
      m_dirtyGroups.push_back(i->second);
    }
  }
}


GfxAsset GfxAssetManager::registerNamedAsset(
  const GfxSemanticName&              name,
        uint32_t                      index) {
  std::unique_lock lock(m_assetLutMutex);

  if (!m_assetLut.insert({ name, GfxAsset(index) }).second)
    Log::err("Asset name ", name.c_str(), " not unique");

  return GfxAsset(index);
}


GfxAssetGroup GfxAssetManager::registerNamedGroup(
  const GfxSemanticName&              name,
        uint32_t                      index) {
  std::unique_lock lock(m_groupLutMutex);

  if (!m_groupLut.insert({ name, GfxAssetGroup(index) }).second)
    Log::err("Asset group name ", name.c_str(), " not unique");

  return GfxAssetGroup(index);
}


bool GfxAssetManager::writeAssetData(
        uint32_t*                     dwords,
        GfxAssetRefType               type,
        GfxAsset                      asset) {
  GfxAssetProperties info = getAsset(asset).iface->getAssetInfo();

  if (info.status != GfxAssetStatus::eResident) {
    info.descriptorIndex = 0u;
    info.gpuAddress = 0ull;
  }

  switch (type) {
    case GfxAssetRefType::eDescriptorIndex: {
      dwords[0] = info.descriptorIndex;
    } break;

    case GfxAssetRefType::eBufferAddress: {
      dwords[0] = uint32_t(info.gpuAddress);
      dwords[1] = uint32_t(info.gpuAddress >> 32);
    } break;
  }

  return info.status == GfxAssetStatus::eResident;
}


uint64_t GfxAssetManager::computeFeedbackBufferSize() const {
  // One counter followed by an array of group handles
  return (m_groups.allocator.getCount() + 1u) * sizeof(uint32_t);
}


GfxBuffer GfxAssetManager::createFeedbackBuffer(
        uint64_t                      size) const {
  GfxBufferDesc desc;
  desc.debugName = "Asset feedback";
  desc.size = size;
  desc.usage = GfxUsage::eShaderStorage |
    GfxUsage::eTransferDst |
    GfxUsage::eTransferSrc;
  return m_device->createBuffer(desc, GfxMemoryType::eAny);
}


void GfxAssetManager::enqueueRequest(
  const GfxAssetRequest&              rq) {
  std::unique_lock lock(m_requestLock);

  m_requestQueue.push(rq);
  m_requestCond.notify_one();
}


void GfxAssetManager::enqueueStreamAsset(
        GfxAsset                      asset) {
  std::unique_lock lock(m_residencyLock);

  m_residencyQueue.push(asset);
  m_residencyCond.notify_one();
}


void GfxAssetManager::executeStreamRequest(
        GfxAssetGroup                 assetGroup) {
  auto& groupInfo = getAssetGroup(assetGroup);

  if (groupInfo.status & GfxAssetGroupStatus::eActive)
    return;

  // Free up some memory if needed. This is especially useful
  // when a large number of new assets is being loaded at once.
  // TODO work out a way to stall stream requests until we can
  // allocate enough memory or are forced to go over budget.
  if (m_gpuMemoryUsed > m_gpuMemoryBudget)
    executeEvictUnusedRequest();

  for (const auto& a : groupInfo.assets) {
    auto& asset = getAsset(a.asset);

    if (!(asset.activeGroupCount++))
      m_unusedAssets.erase({ asset.activeFrameId, a.asset });

    auto assetInfo = asset.iface->getAssetInfo();
    bool makeResident = assetInfo.status == GfxAssetStatus::eEvictRequest;

    if (assetInfo.status == GfxAssetStatus::eNonResident) {
      makeResident = asset.iface->requestStream(GfxAssetManagerIface(this), m_currFrameId);

      if (!makeResident)
        enqueueStreamAsset(a.asset);
    }

    if (makeResident) {
      asset.iface->makeResident(GfxAssetManagerIface(this));
      dirtyAssetGroups(a.asset, m_currFrameId);
    }
  }

  groupInfo.status |= GfxAssetGroupStatus::eActive;
}


void GfxAssetManager::executeEvictRequest(
        GfxAssetGroup                 assetGroup) {
  auto& groupInfo = getAssetGroup(assetGroup);

  if (!(groupInfo.status & GfxAssetGroupStatus::eActive))
    return;

  // Release ownership of all assets and mark them
  // as orphaned if necessary.
  for (const auto& a : groupInfo.assets) {
    auto& asset = getAsset(a.asset);

    if (!(--asset.activeGroupCount)) {
      asset.activeFrameId = m_currFrameId;
      m_unusedAssets.insert({ m_currFrameId, a.asset });
    }
  }

  groupInfo.status -= GfxAssetGroupStatus::eActive;
}


void GfxAssetManager::executeEvictUnusedRequest() {
  // Under memory pressure, aim to always have a small portion of the
  // available memory budget available for eviction immediately so that
  // subsequent resource streaming does not stall. Evict any asset that
  // we can if we're above budget already.
  std::vector<GfxAssetUnusedEntry> newEntries;

  uint64_t memoryTarget = m_gpuMemoryBudget - m_gpuMemoryBudget / 8u;
  uint64_t memoryOrphaned = 0ull;

  for (auto i = m_unusedAssets.begin(); i != m_unusedAssets.end(); ) {
    auto& asset = getAsset(i->asset);

    // Exit early if we're already within budget.
    if (m_gpuMemoryUsed < memoryTarget + memoryOrphaned
     && m_gpuMemoryUsed < m_gpuMemoryBudget)
      break;

    // Ignore assets that are not backed by memory, since
    // evicting them doesn't accomplish anything at all.
    auto assetInfo = asset.iface->getAssetInfo();

    if (!assetInfo.gpuSize) {
      i = m_unusedAssets.erase(i);
      continue;
    }

    switch (assetInfo.status) {
      case GfxAssetStatus::eResident: {
        // Request eviction and add asset to the back of the list with the
        // current frame ID. Since feedback is delayed, the asset may still
        // get accessed in the current frame and we have no way of knowing.
        asset.iface->requestEviction(GfxAssetManagerIface(this), m_currFrameId);
        dirtyAssetGroups(i->asset, m_currFrameId);

        memoryOrphaned += assetInfo.gpuSize;
        newEntries.push_back({ m_currFrameId, i->asset });
        i = m_unusedAssets.erase(i);
      } break;

      case GfxAssetStatus::eEvictRequest: {
        // Evict asset immediately if we can. Otherwise, count it towards already
        // orphaned memory so that we don't end up request eviction for everything.
        if (m_lastFrameId >= i->frameId) {
          asset.iface->evict(GfxAssetManagerIface(this));
          i = m_unusedAssets.erase(i);
        } else {
          memoryOrphaned += assetInfo.gpuSize;
          i++;
        }
      } break;

      default: {
        // Be robust so that the set does not grow indefinitely in case something
        // weird happens. This would indicate a bug in asset status reporting.
        i = m_unusedAssets.erase(i);
      } break;
    }
  }

  // Add assets for which we submitted an eviction request
  // to the set again in order to actually evict them later.
  for (auto& e : newEntries)
    m_unusedAssets.insert(std::move(e));
}


void GfxAssetManager::runRequestWorker() {
  while (true) {
    GfxAssetRequest rq;

    { std::unique_lock lock(m_requestLock);

      m_requestCond.wait(lock, [this] {
        return !m_requestQueue.empty();
      });

      rq = m_requestQueue.front();
      m_requestQueue.pop();
    }

    std::unique_lock lock(m_assetMutex);

    switch (rq.type) {
      case GfxAssetRequestType::eStopWorker:
        return;

      case GfxAssetRequestType::eRequestStream: {
        executeStreamRequest(rq.assetGroup);
      } break;

      case GfxAssetRequestType::eRequestEvict: {
        executeEvictRequest(rq.assetGroup);
      } break;

      case GfxAssetRequestType::eEvictUnused: {
        executeEvictUnusedRequest();
      } break;
    }
  }
}


void GfxAssetManager::runResidencyWorker() {
  while (true) {
    GfxAsset asset;

    { std::unique_lock lock(m_residencyLock);

      m_residencyCond.wait(lock, [this] {
        return !m_residencyQueue.empty();
      });

      asset = m_residencyQueue.front();
      m_residencyQueue.pop();
    }

    // Exit condition
    if (!asset)
      return;

    std::unique_lock lock(m_assetMutex);
    auto& assetIface = getAsset(asset).iface;
    auto assetInfo = assetIface->getAssetInfo();

    if (assetInfo.status == GfxAssetStatus::eStreamRequest
     || assetInfo.status == GfxAssetStatus::eEvictRequest) {
      assetIface->makeResident(GfxAssetManagerIface(this));
      dirtyAssetGroups(asset, m_currFrameId);
    }
  }
}


GfxBufferDesc GfxAssetManager::getGroupBufferDesc() {
  GfxBufferDesc desc;
  desc.debugName = "Asset groups";
  desc.size = 1ull << 20u;
  desc.usage = GfxUsage::eTransferDst | GfxUsage::eShaderStorage | GfxUsage::eShaderResource;
  return desc;
}

}
