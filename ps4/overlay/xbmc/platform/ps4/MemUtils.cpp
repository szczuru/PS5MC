#include "utils/MemUtils.h"
#include <orbis/libkernel.h>

namespace KODI
{
namespace MEMORY
{

void* AlignedMalloc(size_t size, size_t alignment)
{
  void* ptr = nullptr;
  // Prefer DirectMemory for large GPU-related allocations later
  if (posix_memalign(&ptr, alignment, size) != 0)
    return nullptr;
  return ptr;
}

void AlignedFree(void* ptr)
{
  free(ptr);
}

void GetMemoryStatus(MemoryStatus* status)
{
  if (!status)
    return;
  status->totalPhys = 0;
  status->availPhys = 0;
  // TODO: sceKernelAvailableFlexibleMemorySize etc.
}

} // namespace MEMORY
} // namespace KODI
