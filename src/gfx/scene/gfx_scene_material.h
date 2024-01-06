#pragma once

#include "../gfx_shader.h"

#include "gfx_scene_draw.h"
#include "gfx_scene_instance.h"
#include "gfx_scene_node.h"
#include "gfx_scene_pass.h"

namespace as {

/**
 * \brief Material draw arguments
 *
 * Will be passed to draw shaders via push constants.
 */
struct GfxSceneMaterialDrawArgs {
  /** Draw buffer address */
  uint64_t drawListVa;
  /** Render pass buffer address */
  uint64_t passInfoVa;
  /** Pass group buffer address */
  uint64_t passGroupVa;
  /** Instance buffer address */
  uint64_t instanceVa;
  /** Node buffer address */
  uint64_t sceneVa;
  /** Draw group index */
  uint32_t drawGroup;
  /** Current frame ID */
  uint32_t frameId;
};


/**
 * \brief Material shader set
 *
 * Stores a set of shaders for each pass type that the
 * material supports. These shaders will get compiled
 * into a graphics pipeline on material creation.
 */
struct GfxSceneMaterialShaders {
  /** Pass type flags to use this set of shaders for */
  GfxScenePassTypeFlags passTypes = 0u;
  /** Task shader. */
  GfxShader task;
  /** Mesh shader. */
  GfxShader mesh;
  /** Fragment shader. */
  GfxShader fragment;
};


/**
 * \brief Material flags
 *
 * Affects rendering behaviour of a material.
 */
enum class GfxSceneMaterialFlag : uint32_t {
  /** Material is two-sided, and back-face culling
   *  should be disabled. */
  eTwoSided           = (1u << 0),

  eFlagEnum           = 0u
};

using GfxSceneMaterialFlags = Flags<GfxSceneMaterialFlag>;


/**
 * \brief Material description
 *
 * Defines shaders and assets to use for the material.
 */
struct GfxSceneMaterialDesc {
  /** Material name, mostly used for debug purposes. */
  const char* debugName = nullptr;
  /** Material flags. */
  GfxSceneMaterialFlags flags = 0u;
  /** Number of shader pipelines for the material. */
  uint32_t shaderCount = 0u;
  /** Shader pipelines for each supported pass type. */
  const GfxSceneMaterialShaders* shaders = nullptr;
};


/**
 * \brief Material pipeline
 *
 * Stores a pipeline object as well as a render
 * state object to use with that pipeline.
 */
struct GfxSceneMaterialPipeline {
  /** Graphics pipeline object. */
  GfxGraphicsPipeline pipeline;
  /** Render state object. */
  GfxRenderState renderState;
};


/**
 * \brief Material instance
 *
 * Internal representation of a material, including all graphics
 * pipelines and statically assigned assets for the material.
 */
class GfxSceneMaterial {

public:

  GfxSceneMaterial(
    const GfxDevice&                    device,
    const GfxSceneMaterialDesc&         desc);

  ~GfxSceneMaterial();

  /**
   * \brief Binds pipelines and assets to a context for rendering
   *
   * \param [in] context Context object to bind resources to
   * \param [in] passType Pass type to pick the pipeline for
   * \param [in] setIndex Descriptor set index for material assets
   * \returns \c true if the material supports the given pass
   *    type, or \c false if rendering must be skipped.
   */
  bool bindToContext(
    const GfxContext&                   context,
          GfxScenePassType              passType,
          uint32_t                      setIndex) const;

  /**
   * \brief Adjusts draw count for the material
   *
   * Must be called whenever an instance using this material is made
   * resident to increment the draw count, or when an instance is
   * made non-resident to decrement it again with a negative value.
   * \param [in] draws Number of draws to add or remove
   */
  void adjustDrawCount(
          int32_t                       draws) {
    m_drawCount += uint32_t(draws);
  }

  /**
   * \brief Reads current draw count
   *
   * The draw count of a material is used for setting up the draw
   * buffer layout. Only valid if instance residency is not being
   * changed at the same time.
   * \returns Current number of draws for this material.
   */
  uint32_t getDrawCount() const {
    return m_drawCount.load();
  }

private:

  std::array<GfxSceneMaterialPipeline, 8> m_pipelines;
  std::atomic<uint32_t>                   m_drawCount = { 0u };

  static GfxRenderState createRenderState(
    const GfxDevice&                    device,
    const GfxSceneMaterialDesc&         desc);

};


/**
 * \brief Material manager description
 */
struct GfxSceneMaterialManagerDesc {
  /** Descriptor set index to use for material assets. */
  uint32_t materialAssetDescriptorSet = 0;
};


/**
 * \brief Material manager
 *
 * Stores materials with their respective graphics pipelines
 * and asset references, and manages a draw buffer.
 */
class GfxSceneMaterialManager {

public:

  /**
   * \brief Creates material manager
   *
   * \param [in] device Device
   * \param [in] desc Material manager parameters
   */
  GfxSceneMaterialManager(
          GfxDevice                     device,
    const GfxSceneMaterialManagerDesc&  desc);

  ~GfxSceneMaterialManager();

  /**
   * \brief Creates a material
   *
   * \param [in] desc Material description
   * \returns Material index
   */
  uint32_t createMaterial(
    const GfxSceneMaterialDesc&         desc);

  /**
   * \brief Adds draws for a given instance
   *
   * Must be called after an instance is made resident.
   * \param [in] instanceManager Instance manager
   * \param [in] instanceRef Instance reference
   */
  void addInstanceDraws(
    const GfxSceneInstanceManager&      instanceManager,
          GfxSceneNodeRef               instanceRef);

  /**
   * \brief Removes draws for a given instance
   *
   * Must be called when an instance is made non-resident,
   * and must not be called while updating the draw buffer.
   * \param [in] instanceManager Instance manager
   * \param [in] instanceRef Instance reference
   */
  void removeInstanceDraws(
    const GfxSceneInstanceManager&      instanceManager,
          GfxSceneNodeRef               instanceRef);

  /**
   * \brief Sets up draw buffer layout
   *
   * Must not be called while instance residency and per-material
   * draw counts are being changed. Only needs to be called once
   * per frame.
   * \param [in] context Context object
   * \param [in] drawBuffer Draw buffer object
   */
  void updateDrawBuffer(
    const GfxContext&                   context,
          GfxSceneDrawBuffer&           drawBuffer);

  /**
   * \brief Dispatches draws for a given pass
   *
   * Sets up render state for the given pass type and then iterates
   * over all supported materials to dispatch draw calls.
   * \param [in] context Context object
   * \param [in] passManager Render pass manager
   * \param [in] instanceManager Instance manager
   * \param [in] nodeManager Node manager
   * \param [in] passGroup Pass group object
   * \param [in] drawBuffer Draw buffer object
   * \param [in] passType Render pass type
   * \param [in] frameId Current frame number
   */
  void dispatchDraws(
    const GfxContext&                   context,
    const GfxScenePassManager&          passManager,
    const GfxSceneInstanceManager&      instanceManager,
    const GfxSceneNodeManager&          nodeManager,
    const GfxScenePassGroupBuffer&      passGroup,
    const GfxSceneDrawBuffer&           drawBuffer,
          GfxScenePassType              passType,
          uint32_t                      frameId) const;

private:

  GfxDevice                     m_device;
  GfxSceneMaterialManagerDesc   m_desc;

  ObjectMap<GfxSceneMaterial, 8u, 8u> m_materials;
  ObjectAllocator                     m_materialAllocator;

  std::vector<uint32_t>         m_drawCounts;

  void adjustInstanceDraws(
    const GfxSceneInstanceManager&      instanceManager,
          GfxSceneNodeRef               instanceRef,
          int32_t                       adjustment);

};

}
