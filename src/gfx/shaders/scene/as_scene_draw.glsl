#ifndef AS_SCENE_DRAW_H
#define AS_SCENE_DRAW_H

// Draw list header. Stores the layout of the draw buffer.
struct DrawListHeader {
  uint32_t  drawGroupCount;
  uint32_t  drawParameterOffset;
  uint32_t  drawInfoOffset;
  uint32_t  reserved;
};


// Draw list entry. Stores the index of the first draw of this
// draw group within the draw parameters array, and the number
// of draws currently within the group.
struct DrawListEntry {
  uint32_t  drawIndex;
  uint32_t  drawCount;
};


// GPU draw buffer
layout(buffer_reference, buffer_reference_align = 16, scalar)
buffer DrawListBuffer {
  DrawListHeader  header;
  DrawListEntry   drawGroups[];
};


// Buffer used for initializing the GPU draw buffer
layout(buffer_reference, buffer_reference_align = 4, scalar)
readonly buffer DrawListBufferIn {
  DrawListHeader  header;
  uint32_t        drawIndices[];
};


// Draw list entry. Stores information about the instance and
// geometry being drawn, as well as the passes that the instance
// is visible in when rendering multiple passes at once.
struct DrawInstanceInfo {
  uint32_t  instanceAndLod;
  uint16_t  meshIndex;
  uint16_t  meshInstance;
  uint32_t  shadingDataOffset;
  uint32_t  passMask;
};


layout(buffer_reference, buffer_reference_align = 8, scalar)
readonly buffer DrawInstanceInfoBufferIn {
  DrawInstanceInfo  draws[];
};


layout(buffer_reference, buffer_reference_align = 8, scalar)
writeonly buffer DrawInstanceInfoBufferOut {
  DrawInstanceInfo  draws[];
};


// Indirect draw parameter buffer. Stores a flat array of task
// shader workgroup counts.
layout(buffer_reference, buffer_reference_align = 4, scalar)
buffer DrawParameterBuffer {
  u32vec3   draws[];
};


// Inits draw parameters for a given draw group.
void drawListInit(
        DrawListBuffer                drawList,
        uint32_t                      drawGroup) {
  drawList.drawGroups[drawGroup].drawCount = 0;
}


// Adds a set of draw parameters to the draw list.
//
// Increments the draw count for the givend raw group, and writes the
// given draw info and draw parameters to the respective buffers.
void drawListAdd(
        DrawListBuffer                drawList,
        DrawInstanceInfoBufferOut     drawInfos,
        DrawParameterBuffer           drawArgs,
        uint32_t                      drawGroup,
  in    DrawInstanceInfo              drawInfo,
  in    u32vec3                       drawArg) {
  uint32_t index = drawList.drawGroups[drawGroup].drawIndex
                 + atomicAdd(drawList.drawGroups[drawGroup].drawCount, 1u);

  drawInfos.draws[index] = drawInfo;
  drawArgs.draws[index] = drawArg;
}

#endif /* AS_SCENE_DRAW_H */
