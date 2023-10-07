#ifndef AS_SCENE_DRAW_H
#define AS_SCENE_DRAW_H

// Draw list entry. Stores the index of the first draw of this
// draw group within the draw parameters array, and the number
// of draws currently within the group.
struct DrawListEntry {
  uint32_t  drawIndex;
  uint32_t  drawCount;
};

layout(buffer_reference, buffer_reference_align = 8, scalar)
buffer DrawListBuffer {
  DrawListEntry drawGroups[];
};


// Draw list entry. Stores a reference to the geometry node being
// rendered, the LOD to use, as well as the mesh and mesh instance
// to draw.
struct DrawInfo {
  uint32_t  instanceAndLod;
  uint16_t  meshIndex;
  uint16_t  meshInstance;
};

layout(buffer_reference, buffer_reference_align = 8, scalar)
readonly buffer DrawInfoBufferIn {
  DrawInfo  draws[];
};

layout(buffer_reference, buffer_reference_align = 8, scalar)
writeonly buffer DrawInfoBufferOut {
  DrawInfo  draws[];
};


// Indirect draw parameter buffer. Stores a flat array of task
// shader workgroup counts.
layout(buffer_reference, buffer_reference_align = 4, scalar)
buffer DrawParameterBuffer {
  u32vec3   draws[];
};


// Adds a set of draw parameters to the draw list.
//
// Increments the draw count for the givend raw group, and writes the
// given draw info and draw parameters to the respective buffers.
void addDraw(
        DrawListBuffer      drawList,
        DrawInfoBufferOut   drawInfos,
        DrawParameterBuffer drawArgs,
        uint32_t            drawGroup,
  in    DrawInfo            drawInfo,
  in    u32vec3             drawArg) {
  uint32_t index = drawList.drawGroups[drawGroup].drawIndex
                 + atomicAdd(drawList.drawGroups[drawGroup].drawCount, 1u);

  drawInfos.draws[index] = drawInfo;
  drawArgs.draws[index] = drawArg;
}

#endif /* AS_SCENE_DRAW_H */
