// Always enable Vulkan memory model
#pragma use_vulkan_memory_model

// Common extensions
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#extension GL_KHR_memory_scope_semantics : enable

// Enable mesh shader extension as necessary
#if defined(STAGE_TASK) || defined(STAGE_MESH)
#extension GL_EXT_mesh_shader : enable
#endif

// Enable subgroup extensions where they make sense
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_vote : enable

#if defined(STAGE_TASK) || defined(STAGE_MESH) || defined(STAGE_COMP)
#extension GL_KHR_shader_subgroup_clustered : enable
#extension GL_KHR_shader_subgroup_shuffle : enable
#endif

#if defined(STAGE_FRAG)
#extension GL_KHR_shader_subgroup_quad : enable
#extension GL_EXT_demote_to_helper_invocation : enable
#endif

#extension GL_EXT_subgroup_uniform_control_flow : enable

// Common includes
#include "as_common.glsl"
#include "as_matrix.glsl"
#include "as_quaternion.glsl"
#include "as_geometry.glsl"

// Task shader includes
#ifdef STAGE_TASK
#define TASK_PAYLOAD 1
#include "ts_common.glsl"
#endif

// Mesh shader includes
#ifdef STAGE_MESH
#define TASK_PAYLOAD 1
#include "ms_common.glsl"
#endif


// Default FS interface declaration which allows shaders
// to declare a convenient struct containing FS inputs.
#define FS_INPUT_VAR(l, t, n) t n;

// Macros to declare FS input structures
#define FS_DECLARE_INPUT(iface)   \
  struct FsInput {                \
    iface                         \
  }

#define FS_DECLARE_UNIFORM(iface) \
  struct FsUniform {              \
    iface                         \
  }
