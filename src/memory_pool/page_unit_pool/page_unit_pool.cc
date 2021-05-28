/*!
 * Copyright (c) 2019 by Contributors
 * \file src/memory_pool/page_unit_pool/page_unit_pool.cc
 * \brief A memory pool that use page as memory unit
 */
#include <atomic>
#include "mnm/device_api.h"
#include "mnm/memory_pool.h"
#include "mnm/registry.h"

namespace mnm {
namespace memory_pool {
namespace page_unit_pool {

using device_api::DeviceAPI;

/*!
 * \brief A wrapper which holds the a chunck of memory that owned by nobody.
 * The memory chunck hold by this object could be any sizes.
 *
 * The memory could locate on cpu, gpu or any other devices, determined by the device api.
 *
 * \sa NonOwnedMemory
 */
class NonOwnedMemory final : public Memory {
 public:
  explicit NonOwnedMemory(void* data, const Device& dev, std::shared_ptr<DeviceAPI> api) {
    this->data = data;
    this->device = dev;
    this->api = std::move(api);
  }

  ~NonOwnedMemory() {
    if (data != nullptr) {
      api->FreeMemory(data);
    }
  }

 public:
  /*! \brief The pointer to the DeviceAPI which determines the context of memory. */
  std::shared_ptr<DeviceAPI> api;
};

/*!
 * \brief A Memory Pool that organizes the Memory as multiple memory pages. The default size of
 * memory page is 4KB. A memory chunck hold by NonOwnedMemory is composed of one/multiple pages.
 *
 * In this pool, all memory chunck are divide into multiple groups by the number of memory pages.
 * When user request a chunck of memory with size N, the pool will first find whether there is
 * available memory chunck with the same size in this pool. If so, return this available chunck. If
 * not, allocate a new memory chunck with size N, and return it.
 *
 * As the pool hold a reference to each memory chunck allocate, thus the memory chunck won't be
 * freed once it is allocated, until user's application finishes or fails.
 *
 * \example Assume the Page Size is 4KB. When user requests a chunck of memory with size 2KB, the
 * user will actually get a memory chunck with size 4KB, wrapped in NonOwnedMemory.
 *
 * \sa PageUnitPool
 */
class PageUnitPool : public MemoryPool {
 public:
  explicit PageUnitPool(Device dev) {
    this->device = dev;
    this->api = DeviceAPI::Get(dev.device_type);
  }

  int64_t GetAllocBytes(int64_t nbytes) override {
    // round the chunck size to mutlpile page size.
    return !!(nbytes & ((1 << page_size_exp) - 1)) + (nbytes >> page_size_exp) << page_size_exp;
  }

  virtual inline void* AllocDeviceMemory(int64_t nbytes, int64_t alignment) {
    return api->AllocMemory(nbytes, alignment);
  }

  inline float BytesToMegaBytes(float nbytes) {
    return nbytes / 1048576.0;
  }

  float FreeUnusedChunks() {
    // Remove the memory from the pool and return the freed memory in bytes.
    // Since this is the last share_ptr, the removed memory will be deconstructed and freed.
    float total_free = 0.0;

    std::vector<int64_t> page_nbytes;
    for (auto kv : _pool) {
      page_nbytes.push_back(kv.first);
    }

    for (auto nbytes : page_nbytes) {
      std::list<int64_t> unused_idxs;
      for (int i = 0; i < _pool[nbytes].size(); ++i) {
        if (_pool[nbytes][i].use_count() == 1) {
          unused_idxs.push_back(i);
        }
      }
      total_free += nbytes * unused_idxs.size();

      auto it = _pool[nbytes].begin();
      for (int i = 0; !unused_idxs.empty() && it != _pool[nbytes].end(); ++i) {
        if (i == unused_idxs.front()) {
          it = _pool[nbytes].erase(it);
          unused_idxs.pop_front();
        } else {
          ++it;
        }
      }
    }
    return total_free;
  }

  std::shared_ptr<Memory> Alloc(int64_t nbytes, int64_t alignment) override {
    nbytes = GetAllocBytes(nbytes);
    CHECK_GE(nbytes, 0);

    // Find whether there are available memory chuncks in the pool.
    // If so, return the available memory chunck.
    if (_pool.find(nbytes) == _pool.end()) {
      _pool.insert({nbytes, std::vector<std::shared_ptr<Memory>>()});
    }
    for (int i = 0; i < _pool[nbytes].size(); i++) {
      if (_pool[nbytes][i].use_count() == 1) {
        int64_t address = (int64_t)_pool[nbytes][i]->data;
        if (address % alignment == 0) return _pool[nbytes][i];
      }
    }

    // If not, allocate a new memory chunck from device.
    void* data = nullptr;
    if (nbytes > 0) {
      try {
        data = AllocDeviceMemory(nbytes, alignment);
      } catch (const dmlc::Error& e) {
        // Out of memory, free unused chunks on other pages and re-allocate them here.
        auto free_nbytes = FreeUnusedChunks();
        DLOG(WARNING) << "Out-of-memory. Re-organized memory pool and got "
                      << BytesToMegaBytes(free_nbytes) << " MBs";

        // If the freed memory is insufficient, then we can do nothing in memory pool.
        CHECK_LE(nbytes, free_nbytes)
            << "Out-of-memory. Can allocate " << BytesToMegaBytes(free_nbytes)
            << " MBs but require " << BytesToMegaBytes(nbytes) << " MBs";
        data = AllocDeviceMemory(nbytes, alignment);
      }
      std::shared_ptr<Memory> new_mem = std::make_shared<NonOwnedMemory>(data, device, api);
      _pool[nbytes].push_back(new_mem);
      return new_mem;
    } else {
      return std::make_shared<NonOwnedMemory>(data, device, api);
    }
  }

  std::vector<std::shared_ptr<Memory>> AllocBatch(const std::vector<int64_t>& nbytes,
                                                  int64_t alignment) override {
    std::vector<std::shared_ptr<Memory>> ret;
    ret.reserve(nbytes.size());
    for (int64_t bytes : nbytes) {
      ret.emplace_back(Alloc(bytes, alignment));
    }
    return ret;
  }

  std::pair<float, float> GetPoolSize() override {
    float used_total = 0;
    float pool_total = 0;

    // First get all chunk sizes. Note that we do not count the number of used chunks
    // with use_count > 1 here becuase the kv itself also holds a share_ptr.
    std::vector<int64_t> page_nbytes;
    for (auto kv : _pool) {
      page_nbytes.push_back(kv.first);
    }

    // Then directly access each page to get the precise use_count.
    for (auto nbytes : page_nbytes) {
      size_t used_chunks = 0;
      for (int i = 0; i < _pool[nbytes].size(); i++) {
        used_chunks += (_pool[nbytes][i].use_count() > 1) ? 1 : 0;
      }
      used_total += BytesToMegaBytes(nbytes * used_chunks);
      pool_total += BytesToMegaBytes(nbytes * _pool[nbytes].size());
    }
    return std::make_pair(used_total, pool_total);
  }

 public:
  static void* make(const Device& dev) {
    return new PageUnitPool(dev);
  }

 protected:
  Device device;
  /*! \brief The size of each memory page (exponent). */
  static const int64_t page_size_exp = 12;
  /*! \brief The pointer to the DeviceAPI which determines the context of memory. */
  std::shared_ptr<DeviceAPI> api;
  /*! \brief The pool that hold the references to NonOwnedMemory. */
  std::unordered_map<int64_t, std::vector<std::shared_ptr<Memory>>> _pool;
};

/*!
 * \brief A virtual page unit memory pool. This pool has the same behavior as PageUnitPool.
 * However, although the reported pool size is the same as PageUnitPool, it doesn't really
 * allocate any device memory but only creates placeholder pages. Thus, this pool cannot
 * be used for real execution but should only be used for memory profiling.
 *
 * \sa VirtualPageUnitPool
 */
class VirtualPageUnitPool final : public PageUnitPool {
 public:
  explicit VirtualPageUnitPool(Device dev) : PageUnitPool(dev) {
  }

  inline void* AllocDeviceMemory(int64_t nbytes, int64_t alignment) override {
    // Skip the device memory allocation.
    return nullptr;
  }

 public:
  static void* make(const Device& dev) {
    return new VirtualPageUnitPool(dev);
  }
};

MNM_REGISTER_GLOBAL("mnm.memory_pool._make.page_unit_pool")
    .set_body_typed([](const tvm::Device& dev) { return PageUnitPool::make(Device(dev)); });

MNM_REGISTER_GLOBAL("mnm.memory_pool._make.virtual_page_unit_pool")
    .set_body_typed([](const tvm::Device& dev) { return VirtualPageUnitPool::make(Device(dev)); });

}  // namespace page_unit_pool
}  // namespace memory_pool
}  // namespace mnm
