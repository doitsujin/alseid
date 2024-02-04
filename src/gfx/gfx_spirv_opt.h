#pragma once

#include <cstdint>
#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../util/util_flags.h"
#include "../util/util_types.h"

namespace as {

/**
 * \brief SPIR-V instruction
 *
 * Convenience class that helps iterate over the SPIR-V
 * instruction stream and read or replace arguments.
 */
class SpirvInstruction {

public:

  SpirvInstruction() = default;

  SpirvInstruction(uint32_t* ptr)
  : m_ptr(ptr) { }

  /**
   * \brief Queries instruction length
   * \returns Instruction length, in DWORDs
   */
  uint32_t getLength() const {
    return m_ptr[0] >> 16u;
  }

  /**
   * \brief Queries opcode token
   * \returns Opcode token
   */
  uint32_t getOpcode() const {
    return m_ptr[0] & 0xffffu;
  }

  /**
   * \brief Returns reference to operand
   *
   * Note that index 0 points to the opcode token itself.
   * \param [in] index Operand index
   * \returns Reference to the given operand dword
   */
  uint32_t& getOperand(uint32_t index) const {
    return m_ptr[index];
  }

  /**
   * \brief Checks whether instruction pointer is valid
   * \returns \c true if the pointer is valid
   */
  explicit operator bool () const {
    return m_ptr != nullptr;
  }

private:

  uint32_t* m_ptr = nullptr;

};


/**
 * \brief SPIR-V instruction stream iterator
 *
 * Iterates over the instruction stream.
 */
class SpirvIterator {

public:

  using pointer = void;
  using value_type = SpirvInstruction;
  using reference_type = SpirvInstruction;
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::forward_iterator_tag;

  SpirvIterator() = default;

  explicit SpirvIterator(
          uint32_t*                     code)
  : m_curr(code) { }

  SpirvInstruction operator * () const {
    return SpirvInstruction(m_curr);
  }

  SpirvInstruction operator -> () const {
    return SpirvInstruction(m_curr);
  }

  SpirvIterator& operator ++ () {
    m_curr += SpirvInstruction(m_curr).getLength();
    return *this;
  }

  SpirvIterator operator ++ (int) {
    SpirvIterator result = *this;
    m_curr += SpirvInstruction(m_curr).getLength();
    return result;
  }

  bool operator == (const SpirvIterator&) const = default;
  bool operator != (const SpirvIterator&) const = default;

private:

  uint32_t* m_curr = nullptr;

};


/**
 * \brief SPIR-V code buffer
 *
 * Stores SPIR-V code and provides convenience methods
 * to add new instructions and allocate SPIR-V IDs.
 */
class SpirvCodeBuffer {
  constexpr static uint32_t SpirvHeaderDwords     = 5u;
  constexpr static uint32_t SpirvHeaderMagic      = 0u;
  constexpr static uint32_t SpirvHeaderVersion    = 1u;
  constexpr static uint32_t SpirvHeaderGenerator  = 2u;
  constexpr static uint32_t SpirvHeaderBound      = 3u;
  constexpr static uint32_t SpirvHeaderSchema     = 4u;
public:

  /**
   * \brief Creates empty code buffer
   *
   * The resulting code buffer will not be useful
   * for anything until a header is added.
   */
  SpirvCodeBuffer() = default;

  /**
   * \brief Initializes code buffer with header
   *
   * \param [in] version SPIR-V version
   * \param [in] generator Generator
   * \param [in] bound Allocated IDs
   */
  SpirvCodeBuffer(
          uint32_t                      version,
          uint32_t                      generator,
          uint32_t                      bound);

  /**
   * \brief Creates code buffer from existing binary
   *
   * \param [in] codeSize Code size, in bytes
   * \param [in] code SPIR-V binary
   */
  SpirvCodeBuffer(
          size_t                        codeSize,
    const uint32_t*                     code);

  /**
   * \brief Takes ownership of an existing code buffer
   * \param [in] code Code buffer
   */
  SpirvCodeBuffer(
          std::vector<uint32_t>&&       code);

  ~SpirvCodeBuffer();

  /**
   * \brief Queries SPIR-V version
   * \returns SPIR-V version
   */
  uint32_t getVersion() const {
    return m_code[SpirvHeaderVersion];
  }

  /**
   * \brief Queries SPIR-V generator
   * \returns SPIR-V generator
   */
  uint32_t getGenerator() const {
    return m_code[SpirvHeaderGenerator];
  }

  /**
   * \brief Queries number of bound IDs
   * \returns Number of bound IDs
   */
  uint32_t getBoundIDs() const {
    return m_code[SpirvHeaderBound];
  }

  /**
   * \brief Queries code size
   * \returns Code size, in bytes
   */
  size_t getSize() const {
    return m_code.size() * sizeof(uint32_t);
  }

  /**
   * \brief Retrieves pointer to code
   * \returns Pointer to code
   */
  const uint32_t* getCode() const {
    return m_code.data();
  }

  /**
   * \brief Retrieves writable pointer to code
   *
   * Useful for in-line code replacements.
   * \returns Pointer to code
   */
  uint32_t* getCode() {
    return m_code.data();
  }

  /**
   * \brief Allocates a new SPIR-V ID
   * \returns New, unused ID.
   */
  uint32_t allocateId() {
    return m_code[SpirvHeaderBound]++;
  }

  /**
   * \brief Appends instructions to the code buffer
   *
   * Copies all operands without any further modification.
   * \param [in] ins Instruction to copy
   * \returns Inserted instruction. Will be invalidated when
   *    adding further instructions to the code buffer.
   */
  SpirvInstruction addInstruction(
    const SpirvInstruction&             ins) {
    size_t offset = m_code.size();

    for (uint32_t i = 0; i < ins.getLength(); i++)
      m_code.push_back(ins.getOperand(i));

    return SpirvInstruction(&m_code[offset]);
  }

  /**
   * \brief Adds new instruction to the code buffer
   *
   * \param [in] opcode Opcode of the new instruction
   * \param [in] length Instruction length, in dwords
   * \returns Inserted instruction
   */
  SpirvInstruction addInstruction(
          uint32_t                      opcode,
          uint32_t                      length) {
    size_t offset = m_code.size();
    m_code.resize(offset + length);
    m_code[offset] = opcode | (length << 16u);
    return SpirvInstruction(&m_code[offset]);
  }

  /**
   * \brief Returns instruction iterator
   * \returns Beginning of instruction stream
   */
  SpirvIterator begin() {
    return m_code.size() >= SpirvHeaderDwords
      ? SpirvIterator(m_code.data() + SpirvHeaderDwords)
      : SpirvIterator();
  }

  /**
   * \brief Returns end of instruction stream
   * \returns End of instruction stream
   */
  SpirvIterator end() {
    return m_code.size() >= SpirvHeaderDwords
      ? SpirvIterator(m_code.data() + m_code.size())
      : SpirvIterator();
  }

private:

  std::vector<uint32_t> m_code;

};


/**
 * \brief Constant value helper class
 *
 * Stores a 32-bit specialization constant value, but
 * allows interpreting the data as different data types.
 */
struct SpirvConstantValue {

public:

  SpirvConstantValue() = default;

  explicit SpirvConstantValue(uint32_t value)
  : m_value(value) { }

  uint32_t asUint() const {
    return m_value;
  }

  float asFloat() const {
    float f32;
    std::memcpy(&f32, &m_value, sizeof(f32));
    return f32;
  }

  bool asBool() const {
    return m_value != 0u;
  }

private:

  uint32_t  m_value;

};


/**
 * \brief Relevant decorations
 */
enum class SpirvDecorationFlag : uint32_t {
  /** Variable is a built-in input or output */
  eBuiltIn            = (1u << 0),
  /** Variable is a per-primitive input or output */
  ePerPrimitive       = (1u << 1),

  eFlagEnum           = 0u
};

using SpirvDecorationFlags = Flags<SpirvDecorationFlag>;


/**
 * \brief SPIR-V type, constant, or variable declaration
 *
 * Pairs up the declaring instruction with relevant declarations.
 */
struct SpirvDeclaration {
  /** Instruction handle */
  SpirvInstruction ins = SpirvInstruction();
  /** Enabled decorations */
  SpirvDecorationFlags decorations = 0u;
  /** Built-in */
  uint32_t builtIn = 0u;
};


/**
 * \brief Custom SPIR-V optimization passes
 *
 * Implements various passes that work around driver shortcomings
 * or allow us to specialize shaders at runtime to an extent that
 * is not possible with specialization constants alone.
 */
class SpirvOptimizer {

public:

  /**
   * \brief Initializes optimizer with SPIR-V binary
   *
   * Takes ownership of the given SPIR-V code buffer.
   * \param [in] code Code buffer
   */
  SpirvOptimizer(
          SpirvCodeBuffer&&             code);

  ~SpirvOptimizer();

  /**
   * \brief Extracts and takes ownership of the generated code
   * \returns Code buffer
   */
  SpirvCodeBuffer getCodeBuffer() {
    return std::move(m_codeBuffer);
  }

  /**
   * \brief Assigns a specialization constant value
   *
   * Useful for constant-folding and evaluation passes.
   * \param [in] specId Specialization constant ID
   * \param [in] value Constant value
   */
  void setSpecConstant(
          uint32_t                      specId,
          uint32_t                      value) {
    m_specConstants.insert_or_assign(specId,
      SpirvConstantValue(value));
  }

  /**
   * \brief Sets subgroup size range
   *
   * Helps evaluate boolean expressions conditional on the subgroup size.
   * \param [in] minSize Minimum subgroup size supported by the device
   * \param [in] maxSize Maximum subgroup size supported by the device
   */
  void setSubgroupSize(
          uint32_t                      minSize,
          uint32_t                      maxSize) {
    m_minSubgroupSize = minSize;
    m_maxSubgroupSize = maxSize;
  }

  /**
   * \brief Changes output vertex and primitive counts
   *
   * Allows the export counts of a mesh shader to be specified at runtime.
   * \param [in] vertCount New output vertex count
   * \param [in] primCount New output primitive count
   * \returns \c true if the pass succeeded, \c false on error.
   */
  bool adjustMeshOutputCounts(
          uint32_t                      vertCount,
          uint32_t                      primCount);

private:

  SpirvCodeBuffer m_codeBuffer;

  std::unordered_map<uint32_t, SpirvConstantValue> m_specConstants;

  uint32_t m_minSubgroupSize = 1u;
  uint32_t m_maxSubgroupSize = 128u;

  uint32_t locateEntryPoint(
          uint32_t                      executionModel);

  std::unordered_set<uint32_t> getEntryPointVariables(
          uint32_t                      entryPoint,
          uint32_t                      storageClass);

  std::unordered_set<uint32_t> getEntryPointVariables(
          uint32_t                      entryPoint,
          uint32_t                      storageClassCount,
    const uint32_t*                     storageClasses);

  std::unordered_map<uint32_t, SpirvDeclaration> getDeclarations();

};

}
