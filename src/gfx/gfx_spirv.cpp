#include <algorithm>
#include <vector>
#include <iostream>

#include <spirv_cross.hpp>

#include "../util/util_log.h"

#include "gfx_pipeline.h"
#include "gfx_spirv.h"

namespace as {

bool spirvEncodeBinary(
        WrBufferedStream&             output,
        RdMemoryView                  input) {
  RdStream reader(input);
  WrStream writer(output);

  uint32_t dwordCount = input.getSize() / sizeof(uint32_t);
  writer.write(dwordCount);

  // Block of up to 16 compressed dwords, and one control
  // DWORD which stores the compression mode.
  std::array<uint32_t, 16> block = { };
  uint32_t blockControl = 0;
  uint32_t blockSize = 0;

  // The algorithm used is a simple variable-to-fixed compression that
  // encodes up to two consecutive SPIR-V tokens into one DWORD using
  // a small number of different encodings.
  // Compressed tokens are stored in blocks of 16 DWORDs, each preceeded
  // by a single DWORD which stores the layout for each DWORD, two bits
  // each. The supported layouts, are as follows:
  // 0x0: 1x 32-bit;  0x1: 1x 20-bit + 1x 12-bit
  // 0x2: 2x 16-bit;  0x3: 1x 12-bit + 1x 20-bit
  // These layouts are chosen to allow reasonably efficient encoding of
  // opcode tokens, which usually fit into 20 bits, followed by type IDs,
  // which tend to be low as well since most types are defined early.
  bool needsRead = true;

  uint32_t a = 0;
  uint32_t b = 0;

  for (size_t i = 0; i < dwordCount; ) {
    bool success = true;

    uint32_t schema;
    uint32_t encode;

    if (needsRead) {
      success &= reader.read(a);
      needsRead = false;
    }

    if (likely(i + 1 < dwordCount)) {
      success &= reader.read(b);

      // Pick compression mode based on the data layout
      if (a < (1u << 16) && b <= (1u << 16)) {
        schema = 0x2;
        encode = a | (b << 16);
      } else if (a < (1u << 20) && b < (1u << 12)) {
        schema = 0x1;
        encode = a | (b << 20);
      } else if (a < (1u << 12) && b < (1u << 20)) {
        schema = 0x3;
        encode = a | (b << 12);
      } else {
        schema = 0x0;
        encode = a;
        a = b;
      }

      needsRead = schema != 0;
    } else {
      schema = 0x0;
      encode = a;
    }

    // Write control bits and block data
    blockControl |= schema << (blockSize + blockSize);
    block[blockSize++] = encode;

    i += schema ? 2 : 1;

    // If necessary, flush the block
    if (blockSize == block.size() || i == dwordCount) {
      success &= writer.write(blockControl)
              && writer.write(block.data(), blockSize * sizeof(uint32_t));

      blockControl = 0;
      blockSize = 0;
    }

    if (unlikely(!success))
      return false;
  }

  return true;
}


bool spirvDecodeBinary(
        WrMemoryView                  output,
        RdMemoryView                  input) {
  RdStream reader(input);
  WrStream writer(output);

  // The first token stores the number of uncompressed dwords
  uint32_t dwordsTotal = 0;
  uint32_t dwordsWritten = 0;
 
  if (!reader.read(dwordsTotal))
    return false;

  constexpr uint32_t shiftAmounts = 0x0c101420;

  while (dwordsWritten < dwordsTotal) {
    uint32_t blockControl;

    if (unlikely(!reader.read(blockControl)))
      return false;

    bool success = true;

    for (uint32_t i = 0; i < 16 && dwordsWritten < dwordsTotal; i++) {
      uint32_t dword;

      if (unlikely(!reader.read(dword)))
        return false;

      // Use 64-bit integers for some of the operands so we can
      // shift by 32 bits and not handle it as a special cases
      uint32_t schema = (blockControl >> (i << 1)) & 0x3;
      uint32_t shift  = (shiftAmounts >> (schema << 3)) & 0xff;
      uint64_t mask   = ~(~0ull << shift);
      uint64_t encode = dword;

      success &= writer.write(uint32_t(encode & mask));

      if (schema)
        success &= writer.write(uint32_t(encode >> shift));

      dwordsWritten += schema ? 2 : 1;
    }

    if (unlikely(!success))
      return false;
  }

  // Check whether we got bogus data somewhere
  return dwordsWritten == dwordsTotal;
}


size_t spirvGetDecodedSize(
        RdMemoryView                  input) {
  uint32_t dwordsTotal = 0;

  return RdStream(input).read(dwordsTotal)
    ? size_t(dwordsTotal * sizeof(uint32_t))
    : size_t(0);
}





class GfxSpirvCrossReflection : public spirv_cross::Compiler {

public:

  GfxSpirvCrossReflection(
          size_t                        size,
    const void*                         code)
  : spirv_cross::Compiler(reinterpret_cast<const uint32_t*>(code), size / sizeof(uint32_t)) {

  }

  std::optional<GfxShaderDesc> getShaderDesc() const {
    const spirv_cross::SPIREntryPoint* entryPoint = nullptr;

    for (const auto& ep : ir.entry_points) {
      if (ep.second.orig_name == "main")
        entryPoint = &ep.second;
    }
    
    if (!entryPoint) {
      Log::err("SPIR-V: Entry point 'main' not found");
      return std::nullopt;
    }

    // Retrieve basic info about the entry point
    GfxShaderDesc result = { };

    switch (entryPoint->model) {
      case spv::ExecutionModelVertex:
        result.stage = GfxShaderStage::eVertex;
        break;

      case spv::ExecutionModelTessellationControl:
        result.stage = GfxShaderStage::eTessControl;
        break;

      case spv::ExecutionModelTessellationEvaluation:
        result.stage = GfxShaderStage::eTessEval;
        break;

      case spv::ExecutionModelGeometry:
        result.stage = GfxShaderStage::eGeometry;
        break;

      case spv::ExecutionModelFragment:
        result.stage = GfxShaderStage::eFragment;
        break;

      case spv::ExecutionModelGLCompute:
        result.stage = GfxShaderStage::eCompute;
        break;

      case spv::ExecutionModelMeshEXT:
        result.stage = GfxShaderStage::eMesh;
        break;

      case spv::ExecutionModelTaskEXT:
        result.stage = GfxShaderStage::eTask;
        break;

      default:
        Log::warn("SPIR-V: Unhandled execution model ", uint32_t(entryPoint->model));
        return std::nullopt;
    }

    if (gfxShaderStageHasWorkgroupSize(result.stage)) {
      if (entryPoint->workgroup_size.constant) {
        Log::err("SPIR-V: Workgroup size defined as constant, this is currently not supported");
        return std::nullopt;
      } else if (entryPoint->workgroup_size.id_x) {
        result.workgroupSize = Extent3D(
          get<spirv_cross::SPIRConstant>(entryPoint->workgroup_size.id_x).m.c[0].r[0].u32,
          get<spirv_cross::SPIRConstant>(entryPoint->workgroup_size.id_y).m.c[0].r[0].u32,
          get<spirv_cross::SPIRConstant>(entryPoint->workgroup_size.id_z).m.c[0].r[0].u32);
      } else {
        result.workgroupSize = Extent3D(
          entryPoint->workgroup_size.x,
          entryPoint->workgroup_size.y,
          entryPoint->workgroup_size.z);
      }
    }

    // Retrieve shader resources for the given entry point
    std::unordered_set<spirv_cross::VariableID> vars;

    for (auto id : entryPoint->interface_variables)
      vars.insert(id);

    auto resources = get_shader_resources(vars);
    std::vector<BindingEntry> bindings;

    // Process actual resource bindings
    for (const auto& r : resources.separate_samplers) {
      if (!addBinding(bindings, GfxShaderBindingType::eSampler, r))
        return std::nullopt;
    }

    for (const auto& r : resources.uniform_buffers) {
      if (!addBinding(bindings, GfxShaderBindingType::eConstantBuffer, r))
        return std::nullopt;
    }

    for (const auto& r : resources.storage_buffers) {
      GfxShaderBindingType resourceType = GfxShaderBindingType::eStorageBuffer;

      auto meta = ir.meta.find(r.base_type_id);

      if (meta != ir.meta.end() && meta->second.decoration.decoration_flags.get(spv::DecorationNonWritable))
        resourceType = GfxShaderBindingType::eResourceBuffer;

      if (!addBinding(bindings, resourceType, r))
        return std::nullopt;
    }

    for (const auto& r : resources.separate_images) {
      auto type = get_type(r.base_type_id);

      if (!addBinding(bindings, type.image.dim == spv::DimBuffer
        ? GfxShaderBindingType::eResourceBufferView
        : GfxShaderBindingType::eResourceImageView, r))
        return std::nullopt;
    }

    for (const auto& r : resources.storage_images) {
      auto type = get_type(r.base_type_id);

      if (!addBinding(bindings, type.image.dim == spv::DimBuffer
        ? GfxShaderBindingType::eStorageBufferView
        : GfxShaderBindingType::eStorageImageView, r))
        return std::nullopt;
    }

    // Count number of bindings per set for the array heuristic
    std::array<uint32_t, GfxMaxDescriptorSets> bindingsPerSet = { };

    for (const auto& b : bindings) {
      if (b.binding.descriptorSet >= GfxMaxDescriptorSets) {
        Log::err("SPIR-V: Descriptor set ", b.binding.descriptorSet, " exceeds maximum set count ", GfxMaxDescriptorSets);
        return std::nullopt;
      }

      bindingsPerSet[b.binding.descriptorSet]++;
    }

    // Consider a bindings to refer to descriptor arrays if it was declared
    // as a sized array and is the only binding within its set at index 0.
    for (auto& b : bindings) {
      if (b.isDescriptorArray && !b.binding.descriptorIndex
       && bindingsPerSet[b.binding.descriptorSet] == 1)
        b.binding.descriptorCount = 0;
    }

    // Sort bindings by set and descriptor index
    std::sort(bindings.begin(), bindings.end(),
      [] (const BindingEntry& a, const BindingEntry& b) {
      if (a.binding.descriptorSet < b.binding.descriptorSet) return true;
      if (a.binding.descriptorSet > b.binding.descriptorSet) return false;

      return a.binding.descriptorIndex < b.binding.descriptorIndex;
    });

    // Copy unique bindings to the actual shader description with validation
    result.bindings.reserve(bindings.size());
    const GfxShaderBinding* prevBinding = nullptr;

    for (const auto& b : bindings) {
      bool overlapsPrev = prevBinding
        && prevBinding->descriptorSet == b.binding.descriptorSet
        && prevBinding->descriptorIndex == b.binding.descriptorIndex;

      if (b.binding.descriptorIndex + b.binding.descriptorCount > GfxMaxDescriptorsPerSet) {
        Log::err("SPIR-V: Descriptor index ", b.binding.descriptorIndex, " exceeds maximum descriptor count ", GfxMaxDescriptorsPerSet);
        return std::nullopt;
      }

      if (overlapsPrev) {
        if (prevBinding->type != b.binding.type) {
          Log::err("SPIR-V: Descriptor type ", uint32_t(b.binding.type), " of binding ", b.binding.name,
            " does not match type ", uint32_t(prevBinding->type), " of overlapping binding ", prevBinding->name,
            " at ", prevBinding->descriptorSet, ":", prevBinding->descriptorIndex);
          return std::nullopt;
        }
      } else {
        result.bindings.push_back(b.binding);
        prevBinding = &b.binding;
      }
    }

    // Compute required push constant block size
    for (const auto& r : resources.push_constant_buffers)
      result.constantSize = std::max(result.constantSize, getTypeSize(r.base_type_id, 0));

    // Process shader capabilities
    const auto& capabilities = get_declared_capabilities();

    for (auto cap : capabilities) {
      switch (cap) {
        case spv::CapabilitySampleRateShading:
          result.flags |= GfxShaderFlag::eSampleRate;
          break;

        default:
          break;
      }
    }

    return std::make_optional(std::move(result));
  }

private:

  struct BindingEntry {
    GfxShaderBinding binding;
    bool isDescriptorArray = false;
  };

  uint32_t getTypeSize(
          uint32_t                      typeId,
          uint32_t                      matrixStride) const {
    auto type = get_type(typeId);
    auto meta = ir.meta.find(typeId);

    if (meta != ir.meta.end() && meta->second.decoration.decoration_flags.get(spv::DecorationArrayStride)) {
      uint32_t arraySize = 1;

      for (uint32_t i = 0; i < type.array.size(); i++)
        arraySize *= type.array[i];

      return meta->second.decoration.array_stride * arraySize;
    }

    if (matrixStride && type.columns)
      return matrixStride * type.columns;

    uint32_t typeSize = getScalarSize(typeId, type);

    if (type.vecsize > 1)
      typeSize *= type.vecsize;

    return typeSize;
  }


  uint32_t getScalarSize(
          uint32_t                      typeId,
    const spirv_cross::SPIRType&        type) const {
    switch (type.basetype) {
      case spirv_cross::SPIRType::SByte:
      case spirv_cross::SPIRType::UByte:
        return 1;

      case spirv_cross::SPIRType::Short:
      case spirv_cross::SPIRType::UShort:
      case spirv_cross::SPIRType::Half:
        return 2;

      case spirv_cross::SPIRType::Boolean:
      case spirv_cross::SPIRType::Int:
      case spirv_cross::SPIRType::UInt:
      case spirv_cross::SPIRType::Float:
        return 4;

      case spirv_cross::SPIRType::Int64:
      case spirv_cross::SPIRType::UInt64:
      case spirv_cross::SPIRType::Double:
        return 8;

      case spirv_cross::SPIRType::Struct: {
        uint32_t structSize = 0;

        for (uint32_t i = 0; i < type.member_types.size(); i++) {
          uint32_t memberTypeId = type.member_types[i];

          auto meta = ir.meta.find(typeId);

          if (meta == ir.meta.end())
            return 0;

          uint32_t matrixStride = 0;

          if (meta->second.members[i].decoration_flags.get(spv::DecorationMatrixStride))
            matrixStride = meta->second.members[i].matrix_stride;

          uint32_t memberOffset = meta->second.members[i].offset;
          uint32_t memberSize = getTypeSize(memberTypeId, matrixStride);

          structSize = std::max(structSize, memberOffset + memberSize);
        }

        return structSize;
      }

      default:
        return 0;
    }
  }


  bool addBinding(
          std::vector<BindingEntry>&    bindings,
          GfxShaderBindingType          bindingType,
    const spirv_cross::Resource&        resource) const {
    BindingEntry entry = { };
    entry.binding.type = bindingType;

    // Find binding and set index
    auto meta = ir.meta.find(resource.id);

    if (meta != ir.meta.end()) {
      const auto& decoration = meta->second.decoration;

      if (decoration.decoration_flags.get(spv::DecorationDescriptorSet))
        entry.binding.descriptorSet = decoration.set;

      if (decoration.decoration_flags.get(spv::DecorationBinding))
        entry.binding.descriptorIndex = decoration.binding;
    }

    // Figure out whether this is a descriptor array or not
    entry.binding.descriptorCount = 1;

    auto type = get_type(resource.type_id);

    if (!type.array.empty()) {
      entry.isDescriptorArray = type.array.size() == 1;

      for (uint32_t i = 0; i < type.array.size(); i++) {
        if (i < type.array_size_literal.size() && !type.array_size_literal[i]) {
          Log::err("SPIR-V: The size of descriptor arrays must be a literal");
          return false;
        }

        entry.binding.descriptorCount *= type.array[i];
      }
    }

    // Find unique binding name
    if (entry.binding.descriptorCount) {
      entry.binding.name = get_name(resource.id);

      if (entry.binding.name.empty())
        entry.binding.name = get_name(resource.base_type_id);
    }

    bindings.push_back(std::move(entry));
    return true;
  }

};




std::optional<GfxShaderDesc> spirvReflectBinary(
        size_t                        size,
  const void*                         code) {
  GfxSpirvCrossReflection reflection(size, code);
  return reflection.getShaderDesc();
}

}
