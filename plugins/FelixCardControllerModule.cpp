/**
 * @file FelixCardControllerModule.cpp FelixCardControllerModule DAQModule implementation
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#include "flxlibs/felixcardcontroller/Nljs.hpp"
// #include "flxlibs/felixcardcontrollerinfo/InfoNljs.hpp"

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
  : DAQModule(name)
{
  //m_card_wrapper = std::make_unique<CardControllerWrapper>();

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
  
  auto modconf = mcfg->module<appmodel::FelixCardControllerModule>(get_name());

  auto det_connections = modconf->get_controls();

  for( auto det_conn : det_connections )  {

    // Extract felix infos
    auto flx_if = det_conn->get_receiver()->cast<appmodel::FelixInterface>();
    auto det_senders = det_conn->get_senders();
    std::vector<const appmodel::FelixDataSender*> flx_senders;
    std::transform(det_senders.begin(), det_senders.end(), flx_senders.begin(),
      [](const confmodel::DetDataSender* ptr) -> const appmodel::FelixDataSender* {
          return dynamic_cast<const appmodel::FelixDataSender*>(ptr);
      });

    uint32_t id = flx_if->get_card()+flx_if->get_slr();
    m_card_wrappers.emplace(std::make_pair(id,std::make_unique<CardControllerWrapper>(id, flx_if, flx_senders)));
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
    cw->configure();
    cw->check_alignment(aligned);
  }
}

// void
// FelixCardControllerModule::get_info(opmonlib::InfoCollector& ci, int /*level*/)
// {
//   // for (auto lu : m_cfg.logical_units) {
//   //    uint32_t id = m_cfg.card_id+lu.log_unit_id;
//   //    uint64_t aligned = m_card_wrappers.at(id)->get_register(REG_GBT_ALIGNMENT_DONE);

//   //    m_card_wrappers.at(id)->check_alignment(lu, aligned); // check links are aligned

//   //    for( auto li : lu.links) {
//   //       std::stringstream info_name;
//   //       info_name << "device_" << id << "_link_" << li.link_id;
//   //       felixcardcontrollerinfo::LinkInfo info;
//   //       info.device_id = id;
//   //       info.link_id = li.link_id;
//   //       info.enabled = li.enabled;
//   //       info.aligned = aligned & (1<<li.link_id);
//   //       opmonlib::InfoCollector tmp_ic;
//   //       tmp_ic.add(info);
//   //       ci.add(info_name.str(),tmp_ic);
//   //    }
//   // }


//   for( auto const & [id, cw ] : m_card_wrappers ) {

//     uint64_t aligned = cw->get_register(REG_GBT_ALIGNMENT_DONE);
//     cw->check_alignment(aligned);

//   }
// }

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
