#ifndef AS_DRAW_H
#define AS_DRAW_H

#define DRAW_LIST_TS_WORKGROUPS_PER_DRAW  (32768u)

// Draw list header. Stores the layout of the draw buffer.
struct DrawListHeader {
  uint32_t  drawGroupCount;
  uint32_t  drawCountOffset;
  uint32_t  drawParameterOffset;
  uint32_t  drawInfoOffset;
};


// Draw list entry. Stores the index of the first draw of this
// draw group within the draw parameters array, and the number
// of draws currently within the group.
struct DrawListEntry {
  uint32_t  drawIndex;
  uint32_t  drawCount;
  uint32_t  dispatchIndex;
  uint32_t  dispatchCount;
  uint32_t  searchTreeDepth;
  uint32_t  searchTreeCounterIndex;
  u32vec3   searchTreeDispatch;
  uint32_t  searchTreeLayerOffsets[6u];
  uint32_t  taskShaderThreadCount;
};


// GPU draw buffer
layout(buffer_reference, buffer_reference_align = 16, scalar)
queuefamilycoherent buffer DrawListBuffer {
  DrawListHeader  header;
  DrawListEntry   drawGroups[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer DrawListBufferIn {
  DrawListHeader  header;
  DrawListEntry   drawGroups[];
};


// Draw list entry. Stores information about the instance and
// geometry being drawn, as well as the passes that the instance
// is visible in when rendering multiple passes at once.
struct DrawInstanceInfo {
  uint64_t  meshletBufferAddress;
  uint32_t  instanceIndexAndLod;
  uint16_t  instanceDrawIndex;
  uint16_t  meshInstanceCount;
  uint32_t  meshIndex;
  uint32_t  meshletIndex;
  uint32_t  meshletCount;
  uint32_t  passMask;
};


uint32_t csPackInstanceIndexAndLod(uint32_t instanceIndex, uint32_t lod) {
  return bitfieldInsert(instanceIndex, lod, 24, 8);
}

uint32_t csGetPackedInstanceIndexFromDraw(uint32_t packed) {
  return bitfieldExtract(packed, 0, 24);
}

uint32_t csGetPackedLodFromDraw(uint32_t packed) {
  return bitfieldExtract(packed, 24, 8);
}


layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer DrawInstanceInfoBufferIn {
  DrawInstanceInfo  draws[];
};


layout(buffer_reference, buffer_reference_align = 16, scalar)
writeonly buffer DrawInstanceInfoBufferOut {
  DrawInstanceInfo  draws[];
};


// Indirect draw count buffer. Stores a flat array of draw
// counts for each draw group.
layout(buffer_reference, buffer_reference_align = 4, scalar)
writeonly buffer DrawCountBufferOut {
  uint32_t  drawCounts[];
};


// Indirect draw parameter buffer. Stores a flat array of task
// shader workgroup counts.
layout(buffer_reference, buffer_reference_align = 4, scalar)
writeonly buffer DrawParameterBufferOut {
  u32vec3   draws[];
};


// Inits draw parameters for a given draw group.
void drawListInit(
        DrawListBuffer                drawList,
        uint32_t                      drawGroup) {
  drawList.drawGroups[drawGroup].drawCount = 0u;
  drawList.drawGroups[drawGroup].searchTreeDispatch = u32vec3(1u, 1u, 1u);
}


// Adds a set of draw parameters to the draw list.
//
// Increments the draw count for the givend raw group, and writes the
// given draw info and draw parameters to the respective buffers.
void drawListAdd(
        DrawListBuffer                drawList,
        DrawInstanceInfoBufferOut     drawInfos,
        DrawParameterBufferOut        drawArgs,
        uint32_t                      drawGroup,
  in    DrawInstanceInfo              drawInfo) {
  // Allocate draw info entry and write out draw properties
  uint32_t drawIndex = atomicAdd(drawList.drawGroups[drawGroup].drawCount, 1u);
  uint32_t entryIndex = drawList.drawGroups[drawGroup].drawIndex + drawIndex;

  drawInfos.draws[entryIndex] = drawInfo;

  // Adjust dispatch for search tree creation as necessary
  if ((drawIndex % TsWorkgroupSize) == 0u) {
    u32vec2 dispatch = asGetWorkgroupCount2D((drawIndex / TsWorkgroupSize) + 1u);

    atomicMax(drawList.drawGroups[drawGroup].searchTreeDispatch.x, dispatch.x);
    atomicMax(drawList.drawGroups[drawGroup].searchTreeDispatch.y, dispatch.y);
  }
}


// Search tree layers. Each field stores the number of task shader threads
// needed to process the given draw, or set of draws for higher layers.
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer DrawSearchTreeLayerIn {
  uint32_t threadCount[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
writeonly buffer DrawSearchTreeLayerOut {
  uint32_t threadCount[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer DrawSearchTreeLayer {
  queuefamilycoherent
  uint32_t threadCount[];
};

#endif /* AS_DRAW_H */
