#ifndef AS_ASSET_GROUP_H
#define AS_ASSET_GROUP_H

// Asset list buffer header. Stores a frame ID of when the list has
// last changed to allow for dirty tracking, as well as a frame ID
// of when the asset list has last been used for rendering.
struct AssetListHeader {
  uint32_t updateFrameId;
  uint32_t lastUseFrameId;
};

// Asset list reference type. Stores an asset list header, followed
// by a tightly packed dword array 
layout(buffer_reference, buffer_reference_align = 16, scalar)
readonly buffer AssetListBufferIn {
  AssetListHeader header;
  uint32_t        dwords[];
};

layout(buffer_reference, buffer_reference_align = 16, scalar)
writeonly buffer AssetListBufferOut {
  AssetListHeader header;
  uint32_t        dwords[];
};

#endif /* AS_ASSET_GROUP_H */
