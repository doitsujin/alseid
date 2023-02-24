/* This is a modified GLSL version of the GDeflate reference implementation, which can be found at:
 * https://github.com/microsoft/DirectStorage/blob/3a6aeb2e845fafebb58c718d4ee349425d3a3e50/GDeflate/shaders/GDeflate.hlsl
 *
 * SPDX-FileCopyrightText: Copyright (c) 2020, 2021, 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-FileCopyrightText: Copyright (c) Microsoft Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#version 460

#pragma use_vulkan_memory_model

#extension GL_KHR_memory_scope_semantics : require
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_shuffle : require
#extension GL_KHR_shader_subgroup_vote : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

#define NUM_BITSTREAMS 32
#define NUM_THREADS NUM_BITSTREAMS
#define BLOCK_SIZE 65536

layout(local_size_x = NUM_THREADS) in;

// #if (SIMD_WIDTH >= NUM_THREADS)
// #define IN_REGISTER_DECODER
// #endif

// Unlike the original implementation, which uses a fixed
// dispatch size and a work stealing approach, we will
// use indirect dispatches with one workgroup per block.

// Tile info, stores the offset and size of a single
// compressed block in bytes relative to the input stream.
struct tile_t {
  uint32_t offset;
  uint32_t size;
};

// Block buffer. Contains info about all blocks within the
// current stream, the block array can be indexed using the
// workgroup ID.
//
// The first three DWORDs are used as the indirect argument
// buffers, the block array stores byte offsets and sizes.
layout(buffer_reference, buffer_reference_align = 16)
readonly buffer tile_list_t {
  uint32_t tile_count;
  uint32_t reserved0;
  uint32_t reserved1;
  uint32_t uncompressed_size;
  tile_t tiles[];
};

// Aliased binding which gives us access to the raw DWORDs
layout(buffer_reference, buffer_reference_align = 4)
readonly buffer tile_data_t {
  uint32_t dwords[];
};

// Output buffer
layout(buffer_reference, buffer_reference_align = 4)
workgroupcoherent buffer output_data_t {
  uint8_t bytes[];
};

// Dispatch parameters
layout(push_constant)
uniform push_t {
  tile_list_t tile_list;
  output_data_t output_data;
};


// Current input tile address, as well as its
// compressed size in DWORDs
tile_data_t g_tile_va;
uint32_t g_tile_size;


// Data accessor helpers
uint32_t read_output_byte(uint32_t offset) {
  return uint32_t(output_data.bytes[offset]);
}


void store_output_byte(uint32_t offset, uint32_t data) {
  output_data.bytes[offset] = uint8_t(data);
}


uint32_t read_input_dword(uint32_t index) {
  if (index < g_tile_size)
    return g_tile_va.dwords[index];
  else
    return 0;
}


// HLSL helpers
uint32_t shr(uint32_t value, uint32_t n) {
  return n > 31 ? 0 : (value >> n);
}


uint32_t shl(uint32_t value, uint32_t n) {
  return n > 31 ? 0 : (value << n);
}


int32_t firstbithigh(uint32_t n) {
  return findMSB(n);
}


int32_t firstbitlow(uint32_t n) {
  return findLSB(n);
}


uint32_t countbits(uint32_t n) {
  return bitCount(n);
}


uint32_t reversebits(uint32_t n) {
  return bitfieldReverse(n);
}


// Common helpers
uint32_t mask(uint32_t n) {
  return shl(1u, n) - 1u;
}


uint32_t extract(uint32_t data, uint32_t pos, uint32_t n, uint32_t base) {
  return (shr(data, pos) & mask(n)) + base;
}


// Subgroup-related helpers
shared uint32_t g_tmp[NUM_THREADS];


uint32_t ltMask() {
  return gl_SubgroupLtMask.x;
}


uint32_t vote(bool p) {
  return subgroupBallot(p).x;
}


uint32_t shuffle(uint32_t value, uint32_t idx) {
  return subgroupShuffle(value, idx);
}


uint32_t broadcast(uint32_t value, uint32_t idx) {
  return subgroupBroadcast(value, idx);
}


bool all_in_group(bool p) {
  return subgroupAll(p);
}


uint32_t prefix_sum_exclusive(uint32_t value) {
  return subgroupExclusiveAdd(value);
}


uint32_t prefix_sum_inclusive_16(uint32_t value) {
  uint32_t lo = gl_LocalInvocationIndex < 16 ? value : 0;
  uint32_t hi = gl_LocalInvocationIndex < 16 ? 0 : value;

  uint32_t loSum = subgroupInclusiveAdd(lo);
  uint32_t hiSum = subgroupInclusiveAdd(hi);

  return gl_LocalInvocationIndex < 16 ? loSum : hiSum;
}


uint32_t match(uint32_t value) {
  uint32_t mask = 0;

  for (uint i = 0; i < NUM_THREADS; i++)
    mask |= uint32_t(subgroupBroadcast(value, i) == value ? 1u : 0) << i;

  return mask;
}


// Bit reader class. Base offset given in DWORDs.
#define BIT_READER_WIDTH NUM_BITSTREAMS

struct bit_reader_t {
  uint64_t buf;
  uint32_t base;
  uint32_t cnt;
};


void bit_reader_init(out bit_reader_t self) {
  self.buf = uint64_t(read_input_dword(gl_LocalInvocationIndex));
  self.base = BIT_READER_WIDTH;
  self.cnt = BIT_READER_WIDTH;
}


void bit_reader_refill(inout bit_reader_t self, bool p) {
  p = p && self.cnt < BIT_READER_WIDTH;

  uint32_t ballot = vote(p);
  uint32_t offset = countbits(ballot & ltMask());

  if (p) {
    self.buf |= uint64_t(read_input_dword(self.base + offset)) << self.cnt;
    self.cnt += BIT_READER_WIDTH;
  }

  self.base += countbits(ballot);
}


void bit_reader_eat(inout bit_reader_t self, uint32_t n, bool p) {
  if (p) {
    self.buf >>= n;
    self.cnt -= n;
  }

  bit_reader_refill(self, p);
}


uint32_t bit_reader_peek(in bit_reader_t self, uint32_t n) {
  return uint32_t(self.buf) & mask(n);
}


uint32_t bit_reader_peek_32(in bit_reader_t self) {
  return uint32_t(self.buf);
}


uint32_t bit_reader_read(inout bit_reader_t self, uint32_t n, bool p) {
  uint32_t bits = p ? bit_reader_peek(self, n) : 0u;
  bit_reader_eat(self, n, p);
  return bits;
}


// Scratch buffer class
struct scratch_t {
  uint32_t data[64];
};

shared scratch_t g_buf;

void scratch_clear() {
  g_buf.data[gl_LocalInvocationIndex] = 0u;
  g_buf.data[gl_LocalInvocationIndex + 32u] = 0u;
}


uint32_t scratch_get4b(uint32_t i) {
  return (g_buf.data[i / 8] >> (4 * (i % 8))) & 15;
}


void scratch_set4b(uint32_t nibbles, uint32_t n, uint32_t i) {
  nibbles |= (nibbles << 4);
  nibbles |= (nibbles << 8);
  nibbles |= (nibbles << 16);
  nibbles &= ~(int(0xf0000000) >> (28 - n * 4));

  uint32_t base = i / 8;
  uint32_t shift = i % 8;

  atomicOr(g_buf.data[base], shl(nibbles, shift * 4));
  if (shift + n > 8)
    atomicOr(g_buf.data[base + 1], shr(nibbles, (8 - shift) * 4));
}


// Symbol table class
#define SYMTBL_MAX_SYMBOLS (288 + 32)
#define SYMTBL_DISTANCE_CODE_BASE (288)

struct symbol_table_t {
  // Can be stored in uint16_t
  uint32_t symbols[SYMTBL_MAX_SYMBOLS];
};

shared symbol_table_t g_lut;

uint32_t symbol_table_scatter(uint32_t sym, uint32_t len, uint32_t offset) {
  uint32_t mask = match(len);

  if (len != 0)
    g_lut.symbols[offset + countbits(mask & ltMask())] = sym;

  return mask;
}


void symbol_table_init(uint32_t hlit, uint32_t offsets) {
  for (uint32_t i = 0; i < SYMTBL_MAX_SYMBOLS; i += NUM_THREADS) {
    if (gl_LocalInvocationIndex + i < SYMTBL_MAX_SYMBOLS)
      g_lut.symbols[gl_LocalInvocationIndex + i] = 0;
  }

  g_tmp[gl_LocalInvocationIndex] = 0u;

  barrier();

  if (gl_LocalInvocationIndex != 15 && gl_LocalInvocationIndex != 31)
    g_tmp[gl_LocalInvocationIndex + 1] = offsets;

  barrier();

  for (uint32_t i = 0; i < 256 / NUM_THREADS; i++) {
    uint32_t sym = i * NUM_THREADS + gl_LocalInvocationIndex;
    uint32_t len = scratch_get4b(sym);
    uint32_t match = symbol_table_scatter(sym, len, g_tmp[len]);

    if (gl_LocalInvocationIndex == firstbitlow(match))
      g_tmp[len] += countbits(match);

    barrier();
  }

  uint32_t sym = 8 * NUM_THREADS + gl_LocalInvocationIndex;
  uint32_t len = sym < hlit ? scratch_get4b(sym) : 0;
  symbol_table_scatter(sym, len, g_tmp[len]);

  len = scratch_get4b(gl_LocalInvocationIndex + hlit);
  symbol_table_scatter(gl_LocalInvocationIndex, len, SYMTBL_DISTANCE_CODE_BASE + g_tmp[16 + len]);
}

// Decoder class. For simplicity, only the subgroup-optimized implementation was
// ported, however this is expected to be slow on devices with smaller subgroups.
#define DEC_MAX_CODE_LEN (15)

struct decoder_pair_t {
  uint32_t base_code;
  uint32_t offset;
};


uint32_t decoder_pair_offset(in decoder_pair_t self, uint32_t i) {
  return shuffle(self.offset, i);
}


void decoder_pair_init(out decoder_pair_t self, uint32_t counts, uint32_t maxlen) {
  self.offset = prefix_sum_inclusive_16(counts);

  uint32_t base_code = 0;

  for (uint32_t i = 1; i < maxlen; i++) {
    uint lane = gl_LocalInvocationIndex & 15;
    uint count = shuffle(counts, (gl_LocalInvocationIndex & 16) + i);

    if (lane >= i)
      base_code += shl(count, lane - i);
  }

  uint lane = gl_LocalInvocationIndex & 15;
  uint tmp = shl(base_code, 32 - lane);
  self.base_code = (tmp < base_code || lane >= maxlen) ? 0xffffffff : tmp;
}


uint32_t decoder_length_for_code(in decoder_pair_t self, uint32_t code, uint32_t base) {
  uint32_t len = 1;

  if (code >= shuffle(self.base_code, 7 + base))
    len = 8;

  if (code >= shuffle(self.base_code, len + 3 + base))
    len += 4;

  if (code >= shuffle(self.base_code, len + 1 + base))
    len += 2;

  if (code >= shuffle(self.base_code, len + base))
    len += 1;

  return len;
}


uint32_t decoder_id_for_code(in decoder_pair_t self, uint32_t code, uint32_t len, uint32_t base) {
  uint32_t i = len + base - 1;
  return shuffle(self.offset, i) + shr(code - shuffle(self.base_code, i), 32 - len);
}


uint32_t decoder_decode(in decoder_pair_t self, uint32_t bits, out uint32_t len, bool isdist) {
  uint32_t code = reversebits(bits);
  len = decoder_length_for_code(self, code, isdist ? 16 : 0);
  return g_lut.symbols[decoder_id_for_code(self, code, len, isdist ? 16 : 0) + (isdist ? 288 : 0)];
}


uint32_t get_histogram(uint32_t cnt, uint32_t len, uint32_t maxlen) {
  g_tmp[gl_LocalInvocationIndex] = 0;
  barrier();

  if (len != 0 && gl_LocalInvocationIndex < cnt)
    atomicAdd(g_tmp[len], 1);

  barrier();
  return g_tmp[gl_LocalInvocationIndex & 15];
}


void update_histograms(uint32_t len, int i, int n, int hlit) {
  uint32_t cnt = clamp(hlit > i ? hlit - i : 0, 0, n);

  if (cnt != 0)
    atomicAdd(g_tmp[len], cnt);

  cnt = clamp(i + n - hlit, 0, n);

  if (cnt != 0)
    atomicAdd(g_tmp[16 + len], cnt);
}


void clear_histogram() {
  g_tmp[gl_LocalInvocationIndex] = 0;
}


uint32_t read_length_codes(inout bit_reader_t br, uint hclen) {
  const uint lane_for_id[32] = { 3, 17, 15, 13, 11, 9, 7, 5, 4, 6, 8, 10, 12, 14, 16, 18,
                                 0,  1,  2,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0 };

  uint32_t len = bit_reader_read(br, 3, gl_LocalInvocationIndex < hclen);
  len = shuffle(len, lane_for_id[gl_LocalInvocationIndex]);
  len &= gl_LocalInvocationIndex < 19 ? 0xf : 0;
  return len;
}


uint32_t unpack_code_lengths(inout bit_reader_t br, uint32_t hlit, uint32_t hdist, uint32_t hclen) {
  uint len = read_length_codes(br, hclen);

  uint cnts = get_histogram(19, len, 7);
  decoder_pair_t dec;
  decoder_pair_init(dec, cnts, 7);

  symbol_table_scatter(gl_LocalInvocationIndex, len, decoder_pair_offset(dec, len - 1));

  uint32_t count = hlit + hdist;
  uint32_t baseOffset = 0;
  uint32_t lastlen = ~0;

  scratch_clear();
  clear_histogram();

  barrier();

  do {
    uint len;
    uint32_t bits = bit_reader_peek(br, 7 + 7);
    uint sym = decoder_decode(dec, bits, len, false);
    uint idx = sym <= 15 ? 0 : (sym - 15);

    const uint base[4] = {1, 3, 3, 11};
    const uint xlen[4] = {0, 2, 3, 7};

    uint n = base[idx] + (shr(bits, len) & mask(xlen[idx]));
    int lane = firstbithigh(vote(sym != 16) & ltMask());
    uint codelen = sym;

    if (sym > 16)
        codelen = 0;

    uint prevlen = shuffle(codelen, lane);

    if (sym == 16)
      codelen = lane < 0 ? lastlen : prevlen;

    lastlen = broadcast(codelen, NUM_THREADS - 1);

    barrier();

    baseOffset = prefix_sum_exclusive(n) + baseOffset;

    if (baseOffset < count && codelen != 0) {
      update_histograms(codelen, int(baseOffset), int(n), int(hlit));
      scratch_set4b(codelen, n, baseOffset);
    }

    bit_reader_eat(br, len + xlen[idx], baseOffset < count);

    baseOffset = broadcast(baseOffset + n, NUM_THREADS - 1);

    barrier();
  } while (all_in_group(baseOffset < count));

  barrier();
  return g_tmp[gl_LocalInvocationIndex];
}

void write_output(uint32_t dst, uint32_t offset, uint32_t dist, uint32_t length_in, uint32_t byte, bool iscopy) {
  dst += offset;

  if (!iscopy && length_in != 0)
    store_output_byte(dst, byte);

  uint32_t mask = vote(iscopy);
  uint32_t msk = mask;

  while (mask != 0) {
    uint32_t lane = firstbitlow(mask);

    uint32_t off = broadcast(dist, lane);
    uint32_t len = broadcast(length_in, lane);
    uint32_t outval = broadcast(dst, lane);

    controlBarrier(
      gl_ScopeWorkgroup,
      gl_ScopeWorkgroup,
      gl_StorageSemanticsBuffer,
      gl_SemanticsAcquireRelease);

    // Copy using all threads in the wave
    for (uint32_t i = gl_LocalInvocationIndex; i < len; i += NUM_THREADS) {
      uint32_t data = read_output_byte(outval + (i % off) - off);
      store_output_byte(i + outval, data);
    }

    mask &= mask - 1;
  }
}


uint translate_symbol(inout bit_reader_t br, uint32_t sym, uint32_t len, uint32_t bits, bool isdist, bool p) {
  const uint32_t base_dist[] = {
       1,    2,    3,     4,     5,     7,     9,    13,
      17,   25,   33,    49,    65,    97,   129,   193,
     257,  385,  513,   769,  1025,  1537,  2049,  3073,
    4097, 6145, 8193, 12289, 16385, 24577, 32769, 49153 };

  const uint32_t base_length[] = 
    {  0,   3,   4,   5,   6,  7,  8,  9,
      10,  11,  13,  15,  17, 19, 23, 27,
      31,  35,  43,  51,  59, 67, 83, 99,
     115, 131, 163, 195, 227,  3,  0 };

  const uint32_t extra_dist[] = 
    { 0,  0,  0,  0,  1,  1,  2,  2,
      3,  3,  4,  4,  5,  5,  6,  6,
      7,  7,  8,  8,  9,  9, 10, 10,
     11, 11, 12, 12, 13, 13, 14, 14 };

  const uint32_t extra_length[] = 
    { 0, 0, 0, 0, 0,  0, 0, 0,
      0, 1, 1, 1, 1,  2, 2, 2,
      2, 3, 3, 3, 3,  4, 4, 4,
      4, 5, 5, 5, 5, 16, 0 };

  uint32_t base = isdist ? base_dist[sym] : (sym >= 256 ? base_length[sym - 256] : 1);
  uint32_t n = isdist ? extra_dist[sym] : (sym >= 256 ? extra_length[sym - 256] : 0);

  bit_reader_eat(br, len + n, isdist || p);
  return base + (shr(bits, len) & mask(n));
}


uint32_t process_compressed_block(inout bit_reader_t br, uint32_t hlit, uint32_t counts, uint32_t dst) {
  decoder_pair_t dec;
  decoder_pair_init(dec, counts, 15);

  symbol_table_init(hlit, dec.offset);

  uint32_t len;
  uint32_t sym = decoder_decode(dec, bit_reader_peek(br, 15 + 16), len, false);

  uint32_t eob = vote(sym == 256);
  bool oob = (eob & ltMask()) != 0;

  uint32_t value = translate_symbol(br, sym, len, bit_reader_peek_32(br), false, !oob);

  uint32_t length_in = oob ? 0 : value;
  uint32_t offset = prefix_sum_exclusive(length_in);

  bool iscopy = sym > 256;
  uint32_t byte = sym;

  while (eob == 0) {
    sym = decoder_decode(dec, bit_reader_peek(br, 15 + 16), len, iscopy);

    eob = vote(sym == 256);
    oob = (eob & ltMask()) != 0;

    value = translate_symbol(br, sym, len, bit_reader_peek_32(br), iscopy, !oob);
    write_output(dst, offset, value, length_in, byte, iscopy);

    dst += broadcast(offset + length_in, NUM_THREADS - 1);
    barrier();

    length_in = iscopy || oob ? 0 : value;
    offset = prefix_sum_exclusive(length_in);

    iscopy = sym > 256;
    byte = sym;
  }

  sym = decoder_decode(dec, bit_reader_peek(br, 15 + 16), len, true);
  iscopy = iscopy && !oob;

  uint32_t dist = translate_symbol(br, sym, len, bit_reader_peek_32(br), iscopy, false);
  write_output(dst, offset, dist, length_in, byte, iscopy);

  uint32_t res = dst + broadcast(offset + length_in, NUM_THREADS - 1);

  barrier();
  return res;
}


uint32_t process_uncompressed_block(inout bit_reader_t br, uint32_t dst, uint32_t size) {
  uint32_t nrounds = size / NUM_THREADS;

  for (uint32_t i = 0; i < nrounds; i++) {
    store_output_byte(dst + gl_LocalInvocationIndex, bit_reader_read(br, 8, true));
    dst += NUM_THREADS;
  }

  uint32_t rem = size % NUM_THREADS;

  if (rem != 0) {
    uint32_t byte = bit_reader_read(br, 8, gl_LocalInvocationIndex < rem);

    if (gl_LocalInvocationIndex < rem)
        store_output_byte(dst + gl_LocalInvocationIndex, byte);

    dst += rem;
  }

  return dst;
}


uint32_t init_fixed_code_lengths() {
  uint32_t tid = gl_LocalInvocationIndex;
  g_buf.data[tid] = tid < 18 ? 0x88888888 : 0x99999999;
  g_buf.data[tid + 32] = tid < 3 ? 0x77777777 : (tid < 4 ? 0x88888888 : 0x55555555);
  return tid == 7 ? 24 : (tid == 8 ? 152 : (tid == 9 ? 112 : tid == 16 + 5 ? 32 : 0));
}


void main() {
  uint32_t tile_idx = gl_WorkGroupID.x;
  tile_t tile_params = tile_list.tiles[tile_idx];

  g_tile_va = tile_data_t(uint64_t(tile_list) + tile_params.offset);
  g_tile_size = tile_params.size / 4;

  bit_reader_t br;
  bit_reader_init(br);

  uint32_t out_dst = tile_idx * BLOCK_SIZE;
  uint32_t out_size = min(BLOCK_SIZE, tile_list.uncompressed_size - out_dst);

  for (uint32_t i = 0; i < out_size; i += NUM_THREADS) {
    uint32_t offset = i + gl_LocalInvocationIndex;

    if (i < out_size)
      store_output_byte(out_dst + offset, 0);
  }

  bool done;

  do {
    uint32_t header = broadcast(bit_reader_peek_32(br), 0);

    controlBarrier(
      gl_ScopeWorkgroup,
      gl_ScopeWorkgroup,
      gl_StorageSemanticsShared |
      gl_StorageSemanticsBuffer,
      gl_SemanticsAcquireRelease);

    done = extract(header, 0, 1, 0) != 0;
    uint32_t btype = extract(header, 1, 2, 0);

    bit_reader_eat(br, 3, gl_LocalInvocationIndex == 0);

    switch (btype) {
      case 2: {
        bit_reader_eat(br, 14, gl_LocalInvocationIndex == 0);

        uint32_t hlit = extract(header, 3, 5, 257);
        uint32_t hdist = extract(header, 8, 5, 1);
        uint32_t hclen = extract(header, 13, 4, 4);

        uint32_t counts = unpack_code_lengths(br, hlit, hdist, hclen);

        out_dst = process_compressed_block(br, btype == 1 ? 288 : hlit, counts, out_dst);
      } break;

      case 1: {
        uint32_t counts = init_fixed_code_lengths();
        out_dst = process_compressed_block(br, 288, counts, out_dst);
      } break;

      case 0: {
        uint32_t size = broadcast(bit_reader_read(br, 16, gl_LocalInvocationIndex == 0), 0);
        out_dst = process_uncompressed_block(br, out_dst, size);
      } break;

      default:
        // Should never happen
        return;
    }
  } while (!done);
}
