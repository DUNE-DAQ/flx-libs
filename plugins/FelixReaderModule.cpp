/**
 * @file FelixReaderModule.cc FelixReaderModule DAQModule implementation
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#include "confmodel/ResourceSetAND.hpp"
#include "confmodel/Connection.hpp"
#include "confmodel/QueueWithSourceId.hpp"
#include "confmodel/DetectorStream.hpp"
#include "confmodel/DetectorToDaqConnection.hpp"
#include "confmodel/GeoId.hpp"

#include "appmodel/DataReaderModule.hpp"
#include "appmodel/FelixInterface.hpp"
#include "appmodel/FelixDataSender.hpp"

#include "CreateElink.hpp"
#include "FelixReaderModule.hpp"
#include "FelixIssues.hpp"

#include "logging/Logging.hpp"

#include "flxcard/FlxException.h"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "FelixReaderModule" // NOLINT

/**
 * @brief TRACE debug levels used in this source file
 */
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_WORK_STEPS = 10,
  TLVL_BOOKKEEPING = 15
};

namespace dunedaq {
namespace flxlibs {

FelixReaderModule::FelixReaderModule(const std::string& name)
  : DAQModule(name)
  , m_configured(false)
  , m_card_id(0)
  , m_logical_unit(0)
  , m_links_enabled()
  , m_num_links(0)
  , m_block_size(0)
  , m_block_router(nullptr)
//, block_ptr_sinks_{ }

{
 
  register_command("conf", &FelixReaderModule::do_configure);
  register_command("start", &FelixReaderModule::do_start);
  register_command("stop_trigger_sources", &FelixReaderModule::do_stop);
}

void
FelixReaderModule::init(const std::shared_ptr<appfwk::ConfigurationManager> mcfg)
{
  //auto ini = args.get<appfwk::app::ModInit>();
  
  auto modconf = mcfg->get_dal<appmodel::DataReaderModule>(get_name());
  auto session = mcfg->session();

  if (modconf->get_connections().size() != 1) {
    throw InitializationError(ERS_HERE, "FLX Data Reader does not have a unique associated flx_if");
  }

  const confmodel::DetectorToDaqConnection*  det_conn = modconf->get_connections()[0]->cast<confmodel::DetectorToDaqConnection>();

// Create a source_id to local elink map
  std::map<uint, uint> src_id_to_elink_map;
  auto flx_if = det_conn->get_receiver()->cast<appmodel::FelixInterface>();
  auto det_senders = det_conn->get_senders();

  if (!det_senders.empty()) {
    for (const auto& det_sender_res : det_senders) {

      const appmodel::FelixDataSender* data_sender = det_sender_res->cast<appmodel::FelixDataSender>();
      
      if (data_sender != nullptr) {
        // Check if sender enabled
        if (data_sender->disabled(*session))
          continue;

        if (data_sender->get_contains().size() > 1 ){ 
          // TODO add throw
        }

        for (const auto& stream_res : data_sender->get_contains()) {
          const confmodel::DetectorStream* stream = stream_res->cast<confmodel::DetectorStream>();
          if (stream != nullptr) {
            if (stream->disabled(*session)) {
              TLOG_DEBUG(7) << "Ignoring disabled DetectorStream " << stream->UID();
              continue;
            }
            src_id_to_elink_map[stream->get_source_id()] = data_sender->get_link();
          } else {
            // TODO add throw
            // stream is nullpointer, this is not a DetectorStream!
          }
        }

        TLOG(TLVL_BOOKKEEPING) << "Registering link: " << (uint32_t)data_sender->get_link() << " / " << m_links_enabled.size();
        m_links_enabled.push_back(data_sender->get_link());

      } else {
        // TODO add throw
        // det_senders is nullpointer, this is not a FelixDataSender!
        // 
      }
    }
  }
  m_num_links = m_links_enabled.size();
  if (flx_if != nullptr) {
        m_card_wrapper = std::make_unique<CardWrapper>(flx_if, m_links_enabled);
        m_card_id = flx_if->get_card();
        m_logical_unit = flx_if->get_slr();
        m_block_size = flx_if->get_dma_block_size() * m_1kb_block_size;
        m_chunk_trailer_size = flx_if->get_chunk_trailer_size();
  }
  

  for (auto qi : modconf->get_outputs()) {
    auto q_with_id = qi->cast<confmodel::QueueWithSourceId>();
    if (q_with_id == nullptr) continue;
    TLOG_DEBUG(TLVL_WORK_STEPS) << ": CardReader output queue is " << q_with_id->UID();
    TLOG_DEBUG(TLVL_WORK_STEPS) << "Creating ElinkModel for target queue: " << q_with_id->UID() << " DLH number: " << q_with_id->get_source_id();
    auto elink = src_id_to_elink_map[q_with_id->get_source_id()];
    auto link_ptr = m_elinks[elink] = createElinkModel(q_with_id->UID());
    if ( ! link_ptr ) {
      ers::fatal(InitializationError(ERS_HERE, "CreateElink failed to provide an appropriate model for queue!"));
    }
    register_node( q_with_id->UID(), link_ptr);
    link_ptr->init(m_block_queue_capacity);
    //m_elinks[q_with_id->get_source_id()]->init(args, m_block_queue_capacity);
  }

  // Router function of block to appropriate ElinkHandlers
  m_block_router = [&](uint64_t block_addr) { // NOLINT
    // block_counter++;
    const auto* block = const_cast<felix::packetformat::block*>(
      felix::packetformat::block_from_bytes(reinterpret_cast<const char*>(block_addr)) // NOLINT
    );
    auto elink = block->elink;
    if (m_elinks.count(elink) != 0) {
      m_elinks[elink]->queue_in_block_address(block_addr);
    } else {
      // Really bad -> unexpeced ELINK ID in Block.
      // This check is needed in order to avoid dynamically add thousands
      // of ELink parser implementations on the fly, in case the data
      // corruption is extremely severe.
      //
      // Possible causes:
      //   -> enabled links that don't connect to anything
      //   -> unexpected format (fw/sw version missmatch)
      //   -> data corruption from FE
      //   -> data corruption from CR (really rare, last possible cause)

      // NO TLOG_DEBUG, but should count and periodically report corrupted DMA blocks.
    }
  };

  // Set function for the CardWrapper's block processor.
  m_card_wrapper->set_block_addr_handler(m_block_router);
}

void
FelixReaderModule::do_configure(const data_t& /*args*/)
{
   
    bool is_32b_trailer = false;

    TLOG(TLVL_BOOKKEEPING) << "Number of felix links specified in configuration: " << m_links_enabled.size();
    TLOG(TLVL_BOOKKEEPING) << "Number of data link handlers: " << m_elinks.size();

    // Config checks
    if (m_num_links != m_elinks.size()) {
      ers::fatal(ElinkConfigurationInconsistency(ERS_HERE, m_links_enabled.size()));
    }
    if (m_block_size % m_1kb_block_size != 0) {
      ers::fatal(BlockSizeConfigurationInconsistency(ERS_HERE, m_block_size));
    } else if (m_block_size != m_1kb_block_size && m_chunk_trailer_size != m_32b_trailer_size) {
      ers::fatal(BlockSizeConfigurationInconsistency(ERS_HERE, m_block_size));
    } else if (m_chunk_trailer_size == m_32b_trailer_size) {
      is_32b_trailer = true;
    }

    // Configure components
    TLOG(TLVL_WORK_STEPS) << "Card ID: " << m_card_id;
    TLOG(TLVL_WORK_STEPS) << "Configuring components with Block size:" << m_block_size
                          << " & trailer size: " << m_chunk_trailer_size;
    m_card_wrapper->configure();
    // get linkids defined by queues
    std::vector<int> linkids;
    for(auto& [id, elink] : m_elinks) {
      linkids.push_back(id);
    }
    // loop through all elinkmodels, change the linkids to link tags and configure
    for (unsigned i = 0; i < m_num_links; ++i) {
      auto elink = m_elinks.extract(linkids[i]);
      auto tag = m_links_enabled[i] * m_elink_multiplier;
      elink.key() = tag;
      m_elinks.insert(std::move(elink));
      m_elinks[tag]->set_ids(m_card_id, m_logical_unit, m_links_enabled[i], tag);
      m_elinks[tag]->conf(m_block_size, is_32b_trailer);
    }
}

void
FelixReaderModule::do_start(const data_t& /*args*/)
{
    m_card_wrapper->start();
    for (auto& [tag, elink] : m_elinks) {
      elink->start();
    }
}

void
FelixReaderModule::do_stop(const data_t& /*args*/)
{
    m_card_wrapper->stop();
    for (auto& [tag, elink] : m_elinks) {
      elink->stop();
    }
}


} // namespace flxlibs
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::flxlibs::FelixReaderModule)
