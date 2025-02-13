/**
 * @file FelixReaderModule.hpp FELIX card reader DAQ Module.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef FLXLIBS_PLUGINS_FELIXCARDREADER_HPP_
#define FLXLIBS_PLUGINS_FELIXCARDREADER_HPP_

#include "appfwk/cmd/Nljs.hpp"
#include "appfwk/cmd/Structs.hpp"

//#include "flxlibs/felixcardreader/Structs.hpp"

// From appfwk
#include "appfwk/DAQModule.hpp"
#include "appfwk/ConfigurationManager.hpp"

#include "utilities/WorkerThread.hpp"

// FELIX Software Suite provided
#include "packetformat/block_format.hpp"

#include "CardWrapper.hpp"
#include "ElinkConcept.hpp"

#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq::flxlibs {

class FelixReaderModule : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief FelixReaderModule Constructor
   * @param name Instance name for this FelixReaderModule instance
   */
  explicit FelixReaderModule(const std::string& name);

  FelixReaderModule(const FelixReaderModule&) = delete;            ///< FelixReaderModule is not copy-constructible
  FelixReaderModule& operator=(const FelixReaderModule&) = delete; ///< FelixReaderModule is not copy-assignable
  FelixReaderModule(FelixReaderModule&&) = delete;                 ///< FelixReaderModule is not move-constructible
  FelixReaderModule& operator=(FelixReaderModule&&) = delete;      ///< FelixReaderModule is not move-assignable

  void init(const std::shared_ptr<appfwk::ConfigurationManager> mcfg) override;

private:
 
  // Constants
  static constexpr int m_elink_multiplier = 64;
  static constexpr size_t m_block_queue_capacity = 1000000;
  static constexpr size_t m_1kb_block_size = 1024;
  static constexpr int m_32b_trailer_size = 32;

  // Commands
  void do_configure(const data_t& args);
  void do_start(const data_t& args);
  void do_stop(const data_t& args);

  // Configuration
  bool m_configured;
  
  int m_card_id;
  int m_logical_unit;

  std::vector<unsigned int> m_links_enabled;
  unsigned m_num_links;
  std::size_t m_block_size;
  int m_chunk_trailer_size;

  // FELIX Cards
  std::unique_ptr<CardWrapper> m_card_wrapper;

  // ElinkConcept
  std::map<int, std::shared_ptr<ElinkConcept>> m_elinks;

  // Function for routing block addresses from card to elink handler
  std::function<void(uint64_t)> m_block_router; // NOLINT
};

} // namespace dunedaq::flxlibs

#endif // FLXLIBS_PLUGINS_FELIXCARDREADER_HPP_
