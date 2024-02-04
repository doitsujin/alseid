#pragma once

#include "../util/util_iface.h"

namespace as {

/**
 * \brief PCI vendor IDs
 */
struct GfxAdapterVendorId {
  constexpr static uint32_t eAmd      = 0x1002u;
  constexpr static uint32_t eIntel    = 0x8086u;
  constexpr static uint32_t eNvidia   = 0x10deu;
};


/**
 * \brief Adapter info
 */
struct GfxAdapterInfo {
  /** Device name. This is a human-readable string generated
   *  by the backend and should be presented to the user. */
  std::string deviceName;
  /** Driver string. This is a human-readable string and
   *  should only be used for informational purposes. */
  std::string driverInfo;
  /** PCI vendor and device IDs */
  uint32_t vendorId;
  uint32_t deviceId;
  /** Total device memory. On UMA devices, this includes
   *  system memory that is accessible by the GPU. */
  uint64_t totalDeviceMemory;
  /** Total shared memory. This only includes system
   *  memory that is visible to the GPU. */
  uint64_t totalSharedMemory;
};


/**
 * \brief Graphics adapter interface
 *
 * Graphics adapters represent a physical device
 * in the system running on a specific driver.
 */
class GfxAdapterIface {

public:

  virtual ~GfxAdapterIface() { }

  /**
   * \brief Queries adapter properties
   * \returns Adapter properties
   */
  virtual GfxAdapterInfo getInfo() = 0;

};

/** See GfxAdapterIface. */
using GfxAdapter = IfaceRef<GfxAdapterIface>;

}
