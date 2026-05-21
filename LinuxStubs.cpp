// Phase 1 Linux stubs. The full PixelManager + Render layer arrives in Phase 2.
// These stubs satisfy the linker so the math-compute core can be built and
// linked in isolation while the rendering pipeline is being ported.

#ifdef IMAGINA_LINUX

#include "Includes.h"
#include "PixelManager.h"

PixelManager::~PixelManager() {}
void PixelManager::SetResolution(int32_t, int32_t) {}
void PixelManager::Abort() {}
GroupedRasterizingInterface &PixelManager::GetGroupedInterface(size_t) { static GroupedRasterizingInterface *dummy = nullptr; return *dummy; }
void PixelManager::FreeInterface(RasterizingInterface &) {}
void PixelManager::FreeInterface(GroupedRasterizingInterface &) {}

RasterizingInterface::~RasterizingInterface() {}
GroupedRasterizingInterface::~GroupedRasterizingInterface() {}

StandardPixelManager::StandardPixelManager() {}
StandardPixelManager::~StandardPixelManager() {}
void StandardPixelManager::Clear() {}
void StandardPixelManager::SetResolution(int32_t, int32_t) {}
void StandardPixelManager::UpdateRelativeCoordinate(HRReal, HRReal) {}
void StandardPixelManager::RemovePrevTextures() {}
void StandardPixelManager::SetLocation(RelLocation &) {}
void StandardPixelManager::Begin() {}
RasterizingInterface &StandardPixelManager::GetInterface() { static RasterizingInterface *dummy = nullptr; return *dummy; }
GroupedRasterizingInterface &StandardPixelManager::GetGroupedInterface(size_t) { static GroupedRasterizingInterface *dummy = nullptr; return *dummy; }
void StandardPixelManager::FreeInterface(RasterizingInterface &) {}
void StandardPixelManager::FreeInterface(GroupedRasterizingInterface &) {}
bool StandardPixelManager::Completed() { return true; }
void StandardPixelManager::Abort() {}
void StandardPixelManager::GetTextures(TextureDescription *, size_t, size_t &NumObtained) { NumObtained = 0; }
void StandardPixelManager::ChangeMaxit(uint64_t) {}
bool StandardPixelManager::GetProgress(SRReal &Numerator, SRReal &Denoninator) const { Numerator = 0; Denoninator = 1; return true; }

#endif
