#include <spirv_cross.hpp>
#include <fstream>

#include "gfx_spirv_opt.h"

namespace as {

SpirvCodeBuffer::SpirvCodeBuffer(
        size_t                        codeSize,
  const uint32_t*                     code) {
  m_code.resize(codeSize / sizeof(uint32_t));
  std::memcpy(m_code.data(), code, m_code.size() * sizeof(uint32_t));
}


SpirvCodeBuffer::SpirvCodeBuffer(
        uint32_t                      version,
        uint32_t                      generator,
        uint32_t                      bound) {
  m_code.resize(SpirvHeaderDwords);
  m_code[SpirvHeaderMagic] = 0x07230203u;
  m_code[SpirvHeaderVersion] = version;
  m_code[SpirvHeaderGenerator] = generator;
  m_code[SpirvHeaderBound] = bound;
  m_code[SpirvHeaderSchema] = 0u;
}


SpirvCodeBuffer::SpirvCodeBuffer(
        std::vector<uint32_t>&&       code)
: m_code(std::move(code)) {

}


SpirvCodeBuffer::~SpirvCodeBuffer() {

}



SpirvOptimizer::SpirvOptimizer(
          SpirvCodeBuffer&&             code)
: m_codeBuffer(std::move(code)) {

}


SpirvOptimizer::~SpirvOptimizer() {

}


bool SpirvOptimizer::adjustMeshOutputCounts(
        uint32_t                      vertCount,
        uint32_t                      primCount) {
  if (!vertCount && !primCount)
    return true;

  // Find mesh shader entry point
  uint32_t entryPoint = locateEntryPoint(spv::ExecutionModelMeshEXT);

  if (!entryPoint) {
    Log::err("Failed to locate mesh shader entry point");
    return false;
  }

  // Set of variables used by the mesh shader entry point
  std::unordered_set<uint32_t> meshOutputVars = getEntryPointVariables(entryPoint, spv::StorageClassOutput);
  std::unordered_map<uint32_t, SpirvDeclaration> declarations = getDeclarations();

  if (meshOutputVars.empty()) {
    Log::err("Mesh shader entry point has no output variables");
    return false;
  }

  // Start rewriting the code.
  SpirvCodeBuffer code(
    m_codeBuffer.getVersion(),
    m_codeBuffer.getGenerator(),
    m_codeBuffer.getBoundIDs());

  uint32_t uintTypeId = 0u;

  uint32_t vertCountConstId = 0u;
  uint32_t primCountConstId = 0u;

  // Try to find existing integer type
  for (auto decl : declarations) {
    auto& ins = decl.second.ins;

    if (ins && ins.getOpcode() == spv::OpTypeInt &&
        ins.getOperand(2) == 32u && ins.getOperand(3) == 0u) {
      uintTypeId = ins.getOperand(1);
      break;
    }
  }

  for (auto ins : m_codeBuffer) {
    switch (ins.getOpcode()) {
      case spv::OpExecutionMode: {
        auto dst = code.addInstruction(ins);

        if (dst.getOperand(1) == entryPoint) {
          if (dst.getOperand(2) == spv::ExecutionModeOutputVertices && vertCount)
            dst.getOperand(3) = vertCount;
          if (dst.getOperand(2) == spv::ExecutionModeOutputPrimitivesEXT && primCount)
            dst.getOperand(3) = primCount;
        }
      } break;

      case spv::OpVariable: {
        uint32_t typeId = ins.getOperand(1);
        uint32_t varId = ins.getOperand(2);

        if (ins.getOperand(3) == spv::StorageClassOutput) {
          auto varEntry = declarations.find(varId);
          auto ptrEntry = declarations.find(typeId);

          if (varEntry == declarations.end() || ptrEntry == declarations.end()) {
            Log::err("Failed to locate declarations for output variable %", varId);
            return false;
          }

          if (ptrEntry->second.ins.getOpcode() != spv::OpTypePointer) {
            Log::err("Type %", typeId, " for output variable %", varId, " not a pointer");
            return false;
          }

          // Find array type that the given variable type points to
          auto arrayEntry = declarations.find(ptrEntry->second.ins.getOperand(3));

          if (arrayEntry == declarations.end()) {
            Log::err("Base type not found for output variable %", varId);
            return false;
          }

          if (arrayEntry->second.ins.getOpcode() != spv::OpTypeArray) {
            Log::err("Base type of output variable %", varId, " is not an array");
            return false;
          }

          // Rewrite array type, pointer type, and replace the variable type
          uint32_t baseTypeId = arrayEntry->second.ins.getOperand(2);

          if (!vertCountConstId && !primCountConstId) {
            if (!uintTypeId) {
              uintTypeId = code.allocateId();

              auto dst = code.addInstruction(spv::OpTypeInt, 4u);
              dst.getOperand(1) = uintTypeId;
              dst.getOperand(2) = 32u;
              dst.getOperand(3) = 0u;
            }

            if (vertCount) {
              vertCountConstId = code.allocateId();

              auto dst = code.addInstruction(spv::OpConstant, 4u);
              dst.getOperand(1) = uintTypeId;
              dst.getOperand(2) = vertCountConstId;
              dst.getOperand(3) = vertCount;
            }

            if (primCount) {
              primCountConstId = code.allocateId();

              auto dst = code.addInstruction(spv::OpConstant, 4u);
              dst.getOperand(1) = uintTypeId;
              dst.getOperand(2) = primCountConstId;
              dst.getOperand(3) = primCount;
            }
          }

          // Replace array declaration with the correct size. If the given
          // output count is not to be replaced, the ID will be 0.
          uint32_t arraySizeId = (varEntry->second.decorations & SpirvDecorationFlag::ePerPrimitive)
            ? primCountConstId : vertCountConstId;

          if (arraySizeId) {
            uint32_t arrayTypeId = code.allocateId();
            uint32_t ptrTypeId = code.allocateId();

            auto dst = code.addInstruction(spv::OpTypeArray, 4);
            dst.getOperand(1) = arrayTypeId;
            dst.getOperand(2) = baseTypeId;
            dst.getOperand(3) = arraySizeId;

            dst = code.addInstruction(spv::OpTypePointer, 4);
            dst.getOperand(1) = ptrTypeId;
            dst.getOperand(2) = spv::StorageClassOutput;
            dst.getOperand(3) = arrayTypeId;

            typeId = ptrTypeId;
          }
        }

        auto dst = code.addInstruction(ins);
        dst.getOperand(1) = typeId;
      } break;

      default:
        code.addInstruction(ins);
    }
  }

  // Commit local changes
  m_codeBuffer = std::move(code);

  std::ofstream file("/home/philip/out.spv", std::ios::trunc | std::ios::binary);
  file.write(reinterpret_cast<const char*>(m_codeBuffer.getCode()), m_codeBuffer.getSize());

  return true;
}


uint32_t SpirvOptimizer::locateEntryPoint(
        uint32_t                      executionModel) {
  for (auto ins : m_codeBuffer) {
    if (ins.getOpcode() == spv::OpEntryPoint) {
      if (ins.getOperand(1) == executionModel)
        return ins.getOperand(2);
    }

    if (ins.getOpcode() == spv::OpExecutionMode ||
        ins.getOpcode() == spv::OpFunction)
      break;
  }

  return 0u;
}


std::unordered_set<uint32_t> SpirvOptimizer::getEntryPointVariables(
        uint32_t                      entryPoint,
        uint32_t                      storageClass) {
  return getEntryPointVariables(entryPoint, 1, &storageClass);
}


std::unordered_set<uint32_t> SpirvOptimizer::getEntryPointVariables(
        uint32_t                      entryPoint,
        uint32_t                      storageClassCount,
  const uint32_t*                     storageClasses) {
  std::unordered_set<uint32_t> entryPointVars;
  std::unordered_set<uint32_t> result;

  for (auto ins : m_codeBuffer) {
    if (ins.getOpcode() == spv::OpEntryPoint &&
        ins.getOperand(2) == entryPoint) {
      uint32_t index = 3;

      // Skip entry point name. The first DWORD that contains a
      // zero byte is the last DWORD of the string.
      while (index < ins.getLength() &&
          ins.getOperand(index++) >= 0x1000000u)
        continue;

      // All remaining operands are variables
      for (uint32_t i = index; i < ins.getLength(); i++)
        entryPointVars.insert(ins.getOperand(i));
    } else if (ins.getOpcode() == spv::OpVariable) {
      bool matchesStorageClass = storageClassCount == 0u;

      for (uint32_t i = 0; i < storageClassCount && !matchesStorageClass; i++)
        matchesStorageClass = storageClasses[i] == ins.getOperand(3);

      if (matchesStorageClass)
        result.insert(ins.getOperand(2));
    } else if (ins.getOpcode() == spv::OpFunction) {
      // Exit once we reach actual code
      break;
    }
  }

  return result;
}


std::unordered_map<uint32_t, SpirvDeclaration> SpirvOptimizer::getDeclarations() {
  std::unordered_map<uint32_t, SpirvDeclaration> result;

  for (auto ins : m_codeBuffer) {
    switch (ins.getOpcode()) {
      case spv::OpDecorate: {
        auto entry = result.emplace(std::piecewise_construct,
          std::tuple(ins.getOperand(1)), std::tuple()).first;

        switch (ins.getOperand(2)) {
          case spv::DecorationBuiltIn:
            entry->second.decorations = SpirvDecorationFlag::eBuiltIn;
            entry->second.builtIn = ins.getOperand(3);
            break;

          case spv::DecorationPerPrimitiveEXT:
            entry->second.decorations = SpirvDecorationFlag::ePerPrimitive;
            break;

          default:
            result.erase(entry);
            break;
        }
      } break;

      case spv::OpTypeVoid:
      case spv::OpTypeBool:
      case spv::OpTypeInt:
      case spv::OpTypeFloat:
      case spv::OpTypeVector:
      case spv::OpTypeMatrix:
      case spv::OpTypeImage:
      case spv::OpTypeSampler:
      case spv::OpTypeSampledImage:
      case spv::OpTypeArray:
      case spv::OpTypeRuntimeArray:
      case spv::OpTypeStruct:
      case spv::OpTypePointer: {
        auto entry = result.emplace(std::piecewise_construct,
          std::tuple(ins.getOperand(1)), std::tuple()).first;
        entry->second.ins = ins;
      } break;

      case spv::OpConstantTrue:
      case spv::OpConstantFalse:
      case spv::OpConstant:
      case spv::OpConstantComposite:
      case spv::OpConstantNull:
      case spv::OpSpecConstantTrue:
      case spv::OpSpecConstantFalse:
      case spv::OpSpecConstant:
      case spv::OpSpecConstantComposite:
      case spv::OpSpecConstantOp:
      case spv::OpVariable: {
        auto entry = result.emplace(std::piecewise_construct,
          std::tuple(ins.getOperand(2)), std::tuple()).first;
        entry->second.ins = ins;
      } break;

      case spv::OpFunction:
        return result;
    }
  }

  return result;
}

}
