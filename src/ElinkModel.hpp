/**
 * @file ElinkModel.hpp FELIX CR's ELink concept wrapper
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef FLXLIBS_SRC_ELINKMODEL_HPP_
#define FLXLIBS_SRC_ELINKMODEL_HPP_

#include "ElinkConcept.hpp"

#include "flxlibs/opmon/ElinkModel.pb.h"

#include "packetformat/block_format.hpp"

//#include "appfwk/DAQModuleHelper.hpp"
#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"
#include "logging/Logging.hpp"
#include "datahandlinglibs/utils/ReusableThread.hpp"

#include <folly/ProducerConsumerQueue.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace dunedaq::flxlibs {

template<class TargetPayloadType>
class ElinkModel : public ElinkConcept
{
public:
  using sink_t = iomanager::SenderConcept<TargetPayloadType>;
  using err_sink_t = iomanager::SenderConcept<felix::packetformat::chunk>;
  using inherited = ElinkConcept;
  using data_t = nlohmann::json;

  /**
   * @brief ElinkModel Constructor
   * @param name Instance name for this ElinkModel instance
   */
  ElinkModel()
    : ElinkConcept()
    , m_run_marker{ false }
    , m_parser_thread(0)
  {}
  ~ElinkModel() {}

  void set_sink(const std::string& sink_name) override
  {
    if (m_sink_is_set) {
      TLOG_DEBUG(5) << "ElinkModel sink is already set in initialized!";
    } else {
      m_sink_queue = get_iom_sender<TargetPayloadType>(sink_name);
      m_sink_is_set = true;
    }
  }

  std::shared_ptr<sink_t>& get_sink() { return m_sink_queue; }

  std::shared_ptr<err_sink_t>& get_error_sink() { return m_error_sink_queue; }

  void init(const size_t block_queue_capacity)
  {
    m_block_addr_queue = std::make_unique<folly::ProducerConsumerQueue<uint64_t>>(block_queue_capacity); // NOLINT
  }

  void conf(size_t block_size, bool is_32b_trailers)
  {
    if (m_configured) {
      TLOG_DEBUG(5) << "ElinkModel is already configured!";
    } else {
      m_parser_thread.set_name(inherited::m_elink_source_tid, inherited::m_link_tag);
      // if (inconsistency)
      // ers::fatal(ElinkConfigurationInconsistency(ERS_HERE, m_num_links));

      m_parser->configure(block_size, is_32b_trailers); // unsigned bsize, bool trailer_is_32bit
      m_configured = true;
    }
  }

  void start()
  {
    m_t0 = std::chrono::high_resolution_clock::now();
    if (!m_run_marker.load()) {
      set_running(true);
      m_parser_thread.set_work(&ElinkModel::process_elink, this);
      TLOG_DEBUG(5) << "Started ElinkModel of link " << inherited::m_link_id << "...";
    } else {
      TLOG_DEBUG(5) << "ElinkModel of link " << inherited::m_link_id << " is already running!";
    }
  }

  void stop()
  {
    if (m_run_marker.load()) {
      set_running(false);
      while (!m_parser_thread.get_readiness()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      TLOG_DEBUG(5) << "Stopped ElinkModel of link " << m_link_id << "!";
    } else {
      TLOG_DEBUG(5) << "ElinkModel of link " << m_link_id << " is already stopped!";
    }
  }

  void set_running(bool should_run)
  {
    bool was_running = m_run_marker.exchange(should_run);
    TLOG_DEBUG(5) << "Active state was toggled from " << was_running << " to " << should_run;
  }

  bool queue_in_block_address(uint64_t block_addr) // NOLINT(build/unsigned)
  {
    if (m_block_addr_queue->write(block_addr)) { // ok write
      return true;
    } else { // failed write
      return false;
    }
  }


protected:
  void generate_opmon_data() override {

    opmon::CardReaderInfo info;
    auto now = std::chrono::high_resolution_clock::now();
    auto& stats = m_parser_impl.get_stats();

    double seconds = std::chrono::duration_cast<std::chrono::microseconds>(now - m_t0).count() / 1000000.;

    info.set_num_short_chunks_processed( stats.short_ctr.exchange(0) );
    info.set_num_chunks_processed( stats.chunk_ctr.exchange(0) );
    info.set_num_subchunks_processed(stats.subchunk_ctr.exchange(0));
    info.set_num_blocks_processed(stats.block_ctr.exchange(0));

    info.set_rate_blocks_processed(info.num_blocks_processed() / seconds / 1000. );
    info.set_rate_chunks_processed(info.num_chunks_processed() / seconds / 1000. );

    info.set_num_short_chunks_processed_with_error(stats.error_short_ctr.exchange(0));
    info.set_num_chunks_processed_with_error(stats.error_chunk_ctr.exchange(0));
    info.set_num_subchunks_processed_with_error(stats.error_subchunk_ctr.exchange(0));
    info.set_num_blocks_processed_with_error(stats.error_block_ctr.exchange(0));
    info.set_num_subchunk_crc_errors(stats.subchunk_crc_error_ctr.exchange(0));
    info.set_num_subchunk_trunc_errors(stats.subchunk_trunc_error_ctr.exchange(0));
    info.set_num_subchunk_errors(stats.subchunk_error_ctr.exchange(0));


    TLOG_DEBUG(2) << inherited::m_elink_str // Move to TLVL_TAKE_NOTE from readout
		  << " Parser stats ->"
		  << " Blocks: " << info.num_blocks_processed() << " Block rate: " << info.rate_blocks_processed()
		  << " [kHz]"
		  << " Chunks: " << info.num_chunks_processed() << " Chunk rate: " << info.rate_chunks_processed()
		  << " [kHz]"
		  << " Shorts: " << info.num_short_chunks_processed() << " Subchunks:" << info.num_subchunks_processed()
		  << " Error Chunks: " << info.num_chunks_processed_with_error()
		  << " Error Shorts: " << info.num_short_chunks_processed_with_error()
		  << " Error Subchunks: " << info.num_subchunks_processed_with_error()
		  << " Error Block: " << info.num_blocks_processed_with_error();

    m_t0 = now;

    publish( std::move(info),
	     { { "card", std::to_string(m_card_id) },
	       { "logical_unit", std::to_string(m_logical_unit) },
	       { "link", std::to_string(m_link_id) },
	       { "tag", std::to_string(m_link_tag) } } );
	     
  }

private:
  // Types
  using UniqueBlockAddrQueue = std::unique_ptr<folly::ProducerConsumerQueue<uint64_t>>; // NOLINT(build/unsigned)

  // Internals
  std::atomic<bool> m_run_marker;
  bool m_configured{ false };

  // Sink
  bool m_sink_is_set{ false };
  std::shared_ptr<sink_t> m_sink_queue;
  std::shared_ptr<err_sink_t> m_error_sink_queue;

  // blocks to process
  UniqueBlockAddrQueue m_block_addr_queue;

  // Processor
  inline static const std::string m_parser_thread_name = "elinkp";
  datahandlinglibs::ReusableThread m_parser_thread;
  void process_elink()
  {
    while (m_run_marker.load()) {
      uint64_t block_addr;                        // NOLINT
      if (m_block_addr_queue->read(block_addr)) { // read success
        const auto* block = const_cast<felix::packetformat::block*>(
          felix::packetformat::block_from_bytes(reinterpret_cast<const char*>(block_addr)) // NOLINT
        );
        m_parser->process(block);
      } else { // couldn't read from queue
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  }
};

} // namespace dunedaq::flxlibs

#endif // FLXLIBS_SRC_ELINKMODEL_HPP_
