/**
 * @file CardWrapper.hpp FELIX's FlxCard library wrapper
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef FLXLIBS_SRC_CARDWRAPPER_HPP_
#define FLXLIBS_SRC_CARDWRAPPER_HPP_

//#include "flxlibs/felixcardreader/Nljs.hpp"
//#include "flxlibs/felixcardreader/Structs.hpp"

#include "appmodel/FelixInterface.hpp"
#include "datahandlinglibs/utils/ReusableThread.hpp"

#include "flxcard/FlxCard.h"
#include "packetformat/block_format.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace dunedaq::flxlibs {

class CardWrapper
{
public:
  /**
   * @brief CardWrapper Constructor
   * @param name Instance name for this CardWrapper instance
   */
  CardWrapper(const appmodel::FelixInterface * cfg);
  ~CardWrapper();
  CardWrapper(const CardWrapper&) = delete;            ///< CardWrapper is not copy-constructible
  CardWrapper& operator=(const CardWrapper&) = delete; ///< CardWrapper is not copy-assignable
  CardWrapper(CardWrapper&&) = delete;                 ///< CardWrapper is not move-constructible
  CardWrapper& operator=(CardWrapper&&) = delete;      ///< CardWrapper is not move-assignable

  void configure();
  void start();
  void stop();
  void set_running(bool should_run);

  void graceful_stop();

  void set_block_addr_handler(std::function<void(uint64_t)>& handle) // NOLINT(build/unsigned)
  {                                                                  // NOLINT
    m_handle_block_addr = std::bind(handle, std::placeholders::_1);
    m_block_addr_handler_available = true;
  }

private:
  
  // Constants
  static constexpr size_t m_max_links_per_card = 6;
  // static constexpr size_t m_margin_blocks = 4;
  // static constexpr size_t m_block_threshold = 256;
  static constexpr size_t m_block_size = 4096; // felix::packetformat::BLOCKSIZE;
  static constexpr size_t m_dma_wraparound = FLX_DMA_WRAPAROUND;

  // Card
  void open_card();
  void close_card();

  // DMA
  int allocate_CMEM(uint8_t numa, u_long bsize, u_long* paddr, u_long* vaddr); // NOLINT
  void init_DMA();
  void start_DMA();
  void stop_DMA();
  uint64_t bytes_available(); // NOLINT
  void read_current_address();

  // Configuration and internals
  
  std::atomic<bool> m_run_marker;
  bool m_configured{ false };
  uint8_t m_card_id;      // NOLINT
  uint8_t m_logical_unit; // NOLINT
  std::string m_card_id_str;
  uint8_t m_dma_id;         // NOLINT
  size_t m_margin_blocks;   // NOLINT
  size_t m_block_threshold; // NOLINT
  bool m_interrupt_mode;    // NOLINT
  size_t m_poll_time;       // NOLINT
  uint8_t m_numa_id;        // NOLINT
  std::vector<unsigned int> m_links_enabled;      // NOLINT
  std::string m_info_str;

  // Card object
  using UniqueFlxCard = std::unique_ptr<FlxCard>;
  UniqueFlxCard m_flx_card;
  std::mutex m_card_mutex;

  // DMA: CMEM
  std::size_t m_dma_memory_size; // size of CMEM (driver) memory to allocate
  int m_cmem_handle;             // handle to the DMA memory block
  uint64_t m_virt_addr;          // NOLINT virtual address of the DMA memory block
  uint64_t m_phys_addr;          // NOLINT physical address of the DMA memory block
  uint64_t m_current_addr;       // NOLINT pointer to the current write position for the card
  unsigned m_read_index;         // NOLINT
  u_long m_destination;          // u_long -> FlxCard.h

  // Processor
  inline static const std::string m_dma_processor_name = "flx-dma";
  std::atomic<bool> m_run_lock;
  datahandlinglibs::ReusableThread m_dma_processor;
  std::function<void(uint64_t)> m_handle_block_addr; // NOLINT
  bool m_block_addr_handler_available{ false };
  void process_DMA();
};

} // namespace dunedaq::flxlibs

#endif // FLXLIBS_SRC_CARDWRAPPER_HPP_
