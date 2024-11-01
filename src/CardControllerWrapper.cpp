/**
 * @file CardControllerWrapper.cpp
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
// From Module
#include "CardControllerWrapper.hpp"
#include "FelixDefinitions.hpp"
#include "FelixIssues.hpp"

#include "logging/Logging.hpp"
#include "appmodel/FelixDataSender.hpp"
#include "appmodel/FelixInterface.hpp"

#include "flxcard/FlxException.h"

// From STD
#include <iomanip>
#include <memory>
#include <string>

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

CardControllerWrapper::CardControllerWrapper(uint32_t device_id, const appmodel::FelixInterface * flx_cfg, const std::vector<const appmodel::FelixDataSender*>& flx_senders) : 
m_device_id(device_id),
m_flx_cfg(flx_cfg),
m_flx_senders(flx_senders)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
    << "CardControllerWrapper constructor called. Open card " << m_device_id;
	
  m_flx_card = std::make_unique<FlxCard>();
  if (m_flx_card == nullptr) {
    ers::fatal(flxlibs::CardError(ERS_HERE, "Couldn't create FlxCard object."));
  }
  open_card();
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << "CardControllerWrapper constructed.";

}

CardControllerWrapper::~CardControllerWrapper()
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
    << "CardControllerWrapper destructor called. First stop check, then closing card.";
  close_card();
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << "CardControllerWrapper destroyed.";
}

void
CardControllerWrapper::init() {

 // Card initialization
 // this is complicated....should we repeat all code in flx_init?
 // For now do not do the configs of the clock chips
 const std::lock_guard<std::mutex> lock(m_card_mutex);
 m_flx_card->cfg_set_option( BF_MMCM_MAIN_LCLK_SEL, 1 ); // local clock
 m_flx_card->soft_reset();
 //si5328_configure();
 //si5345_configure(0);
 m_flx_card->cfg_set_option(BF_GBT_SOFT_RESET, 0xFFFFFFFFFFFF);
 m_flx_card->cfg_set_option(BF_GBT_SOFT_RESET, 0);

 int bad_channels = m_flx_card->gbt_setup( FLX_GBT_ALIGNMENT_ONE, FLX_GBT_TMODE_FEC ); //What does this do?
 if(bad_channels) {
    TLOG()<< bad_channels << " not aligned.";
 }
 m_flx_card->irq_disable( ALL_IRQS );
 
}

void
CardControllerWrapper::configure()
{

 // Disable all links
 for(size_t i=0 ; i<12; ++i) {
  std::stringstream ss;
  ss << "DECODING_LINK" << std::setw(2) << std::setfill('0') << i << "_EGROUP0_CTRL_EPATH_ENA";
  set_bitfield(ss.str(), 0);
 }


  // Enable/disable emulation
  if(m_flx_cfg->get_emu_fanout()) {
    //set_bitfield("FE_EMU_LOGIC_IDLES", 0); // FIXME
    //set_bitfield("FE_EMU_LOGIC_CHUNK_LENGTH", 0);
    //set_bitfield("FE_EMU_LOGIC_ENA", 0);
    //set_bitfield("FE_EMU_LOGIC_L1A_TRIGGERED", 0);

    set_bitfield("GBT_TOFRONTEND_FANOUT_SEL", 0);
    set_bitfield("GBT_TOHOST_FANOUT_SEL", 0xffffff);
    set_bitfield("FE_EMU_ENA_EMU_TOFRONTEND", 0);
    set_bitfield("FE_EMU_ENA_EMU_TOHOST", 1);
  }
  else {
    //set_register("FE_EMU_LOGIC_ENA", 0);
    //set_register("FE_EMU_LOGIC_L1A_TRIGGERED", 0);
    //set_register("FE_EMU_LOGIC_IDLES", 0);
    //set_register("FE_EMU_LOGIC_CHUNK_LENGTH", 0);

    set_bitfield("FE_EMU_ENA_EMU_TOFRONTEND", 0);
    set_bitfield("FE_EMU_ENA_EMU_TOHOST", 0);
    set_bitfield("GBT_TOFRONTEND_FANOUT_SEL", 0);
    set_bitfield("GBT_TOHOST_FANOUT_SEL", 0);
  }
  // Enable and configure the right links
 

  for(auto s : m_flx_senders) {
    std::stringstream sc_stream;
    sc_stream << "SUPER_CHUNK_FACTOR_LINK_"<< std::setw(2) << std::setfill('0') << s->get_link();
    std::stringstream ena_stream;
    ena_stream<< "DECODING_LINK" << std::setw(2) << std::setfill('0') << s->get_link() << "_EGROUP0_CTRL_EPATH_ENA";
    set_bitfield(sc_stream.str(), m_flx_cfg->get_super_chunk_size());
    set_bitfield(ena_stream.str(), 1);
  }
}

void
CardControllerWrapper::open_card()
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Opening FELIX card " << m_device_id;
  try {
    const std::lock_guard<std::mutex> lock(m_card_mutex);
    m_flx_card->card_open(static_cast<int>(m_device_id), LOCK_NONE); // FlxCard.h
  } catch (FlxException& ex) {
    ers::error(flxlibs::CardError(ERS_HERE, ex.what()));
    exit(EXIT_FAILURE);
  }
}

void
CardControllerWrapper::close_card()
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Closing FELIX card " << m_device_id;
  try {
    const std::lock_guard<std::mutex> lock(m_card_mutex);
    m_flx_card->card_close();
  } catch (FlxException& ex) {
    ers::error(flxlibs::CardError(ERS_HERE, ex.what()));
    exit(EXIT_FAILURE);
  }
}

uint64_t // NOLINT(build/unsigned)
CardControllerWrapper::get_register(std::string key)
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Reading value of register " << key;
  const std::lock_guard<std::mutex> lock(m_card_mutex);
  auto reg_val = m_flx_card->cfg_get_reg(key.c_str());
  return reg_val;
}

void
CardControllerWrapper::set_register(std::string key, uint64_t value) // NOLINT(build/unsigned)
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Setting value of register " << key << " to " << value;
  const std::lock_guard<std::mutex> lock(m_card_mutex);
  m_flx_card->cfg_set_reg(key.c_str(), value);
}

uint64_t // NOLINT(build/unsigned)
CardControllerWrapper::get_bitfield(std::string key)
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Reading value of bitfield " << key;
  const std::lock_guard<std::mutex> lock(m_card_mutex);
  auto bf_val = m_flx_card->cfg_get_option(key.c_str(), false);
  return bf_val;
}

void
CardControllerWrapper::set_bitfield(std::string key, uint64_t value) // NOLINT(build/unsigned)
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Setting value of bitfield " << key << " to " << value;;
  const std::lock_guard<std::mutex> lock(m_card_mutex);
  m_flx_card->cfg_set_option(key.c_str(), value, false);
}

void
CardControllerWrapper::gth_reset()
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Resetting GTH";
  const std::lock_guard<std::mutex> lock(m_card_mutex);
  for (auto i=0 ; i< 6; ++i) {
      m_flx_card->gth_rx_reset(i);
  }    
}

void
CardControllerWrapper::check_alignment( uint64_t aligned )
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Checking link alignment for " << m_flx_cfg->get_slr();
  // std::vector<uint32_t> alignment_mask = lu_cfg.ignore_alignment_mask;
  // check the alingment on a logical unit
  for(auto s : m_flx_senders) {
    // here we want to print out a log message when the links do not appear to be aligned.
    // for WIB readout link_id 5 is always reserved for tp links, so alignemnt is not expected fort these
    bool is_aligned = aligned & (1<<s->get_link());
    // auto found_link = std::find(std::begin(alignment_mask), std::end(alignment_mask), li.link_id);
    // if(found_link == std::end(alignment_mask)) {
    //   if(!lu_cfg.emu_fanout && !is_aligned) {
    //     ers::error(flxlibs::ChannelAlignment(ERS_HERE, li.link_id));
    //   }
    // }
    if(!m_flx_cfg->get_emu_fanout() && !is_aligned) {
      ers::error(flxlibs::ChannelAlignment(ERS_HERE, s->get_link()));
    }
  }
}

} // namespace flxlibs
} // namespace dunedaq
