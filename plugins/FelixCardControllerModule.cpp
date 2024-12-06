/**
 * @file FelixCardControllerModule.cpp FelixCardControllerModule DAQModule implementation
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#include "flxlibs/felixcardcontroller/Nljs.hpp"

#include "FelixCardControllerModule.hpp"
#include "FelixIssues.hpp"

#include "confmodel/DetectorToDaqConnection.hpp"
#include "appmodel/FelixCardControllerModule.hpp"
#include "appmodel/FelixInterface.hpp"
#include "appmodel/FelixDataSender.hpp"

#include "logging/Logging.hpp"

#include <nlohmann/json.hpp>

#include <bitset>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>
#include <utility>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "FelixCardControllerModule" // NOLINT

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

FelixCardControllerModule::FelixCardControllerModule(const std::string& name)
  : DAQModule(name), m_cfg(nullptr)
{

  register_command("conf", &FelixCardControllerModule::do_configure);
  register_command("start", &FelixCardControllerModule::gth_reset);
  register_command("getregister", &FelixCardControllerModule::get_reg);
  register_command("setregister", &FelixCardControllerModule::set_reg);
  register_command("getbitfield", &FelixCardControllerModule::get_bf);
  register_command("setbifield", &FelixCardControllerModule::set_bf);
  register_command("gthreset", &FelixCardControllerModule::gth_reset);
}

void
FelixCardControllerModule::init(const std::shared_ptr<appfwk::ModuleConfiguration> mcfg) {
  
  m_cfg = mcfg->module<appmodel::FelixCardControllerModule>(get_name());
  auto session = mcfg->configuration_manager()->session();

  auto det_connections = m_cfg->get_controls();

  for( auto det_conn : det_connections )  {

    // Extract felix infos
    auto flx_if = det_conn->get_receiver()->cast<appmodel::FelixInterface>();

    auto det_senders = det_conn->get_senders();

    std::vector<const appmodel::FelixDataSender*> flx_senders;
    for( auto ds : det_senders) {

      if (ds->disabled(*session))
        continue;

      flx_senders.push_back(ds->cast<appmodel::FelixDataSender>());
    }

    uint32_t id = flx_if->get_card()+flx_if->get_slr();

    auto cw_p = std::make_shared<CardControllerWrapper>(id, flx_if, flx_senders);
    m_card_wrappers[id] = cw_p;
    register_node( fmt::format("controller-{}", id), cw_p);

    if(m_card_wrappers.size() == 1) {
      // Do the init only for the first device (whole card)
      m_card_wrappers.begin()->second->init();
    }
  }
}

void
FelixCardControllerModule::do_configure(const data_t& args)
{

  for( auto const & [id, cw ] : m_card_wrappers ) {
    uint64_t aligned = cw->get_register(REG_GBT_ALIGNMENT_DONE);
    cw->configure(m_cfg->get_super_chunk_size(), m_cfg->get_emu_fanout());
    cw->check_alignment(aligned);
  }
}

void
FelixCardControllerModule::get_reg(const data_t& args)
{
  auto conf = args.get<felixcardcontroller::GetRegisters>();
  auto id = conf.card_id + conf.log_unit_id;
  for (auto reg_name : conf.reg_names) {
    auto reg_val = m_card_wrappers.at(id)->get_register(reg_name);
    TLOG() << reg_name << "        0x" << std::hex << reg_val;
  }
}

void
FelixCardControllerModule::set_reg(const data_t& args)
{
  auto conf = args.get<felixcardcontroller::SetRegisters>();
  auto id = conf.card_id + conf.log_unit_id;

  for (auto p : conf.reg_val_pairs) {
    m_card_wrappers.at(id)->set_register(p.reg_name, p.reg_val);
  }
}

void
FelixCardControllerModule::get_bf(const data_t& args)
{
  auto conf = args.get<felixcardcontroller::GetBFs>();
  auto id = conf.card_id + conf.log_unit_id;

  for (auto bf_name : conf.bf_names) {
    auto bf_val = m_card_wrappers.at(id)->get_bitfield(bf_name);
    TLOG() << bf_name << "        0x" << std::hex << bf_val;
  }
}

void
FelixCardControllerModule::set_bf(const data_t& args)
{
  auto conf = args.get<felixcardcontroller::SetBFs>();
  auto id = conf.card_id + conf.log_unit_id;

  for (auto p : conf.bf_val_pairs) {
    m_card_wrappers.at(id)->set_bitfield(p.reg_name, p.reg_val);
  }
}

void
FelixCardControllerModule::gth_reset(const data_t& /*args*/)
{
  // Do the reset only for the first device (whole card)
  m_card_wrappers.begin()->second->gth_reset();
}

} // namespace flxlibs
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::flxlibs::FelixCardControllerModule)
