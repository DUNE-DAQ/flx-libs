/**
 * @file FelixCardControllerModule.hpp FELIX card controller DAQ Module
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef FLXLIBS_PLUGINS_FELIXCARDCONTROLLER_HPP_
#define FLXLIBS_PLUGINS_FELIXCARDCONTROLLER_HPP_

#include "appfwk/cmd/Structs.hpp"

// From appfwk
#include "appfwk/DAQModule.hpp"

#include "CardControllerWrapper.hpp"

#include <memory>
#include <string>

namespace dunedaq {
namespace appmodel {

class FelixCardControllerModule;

} // namespace appmodel

namespace flxlibs {

class FelixCardControllerModule : public dunedaq::appfwk::DAQModule
{
public:
  explicit FelixCardControllerModule(const std::string& name);

  FelixCardControllerModule(const FelixCardControllerModule&) = delete; ///< FelixCardControllerModule is not copy-constructible
  FelixCardControllerModule& operator=(const FelixCardControllerModule&) = delete; ///< FelixCardControllerModule is not copy-assignable
  FelixCardControllerModule(FelixCardControllerModule&&) = delete;            ///< FelixCardControllerModule is not move-constructible
  FelixCardControllerModule& operator=(FelixCardControllerModule&&) = delete; ///< FelixCardControllerModule is not move-assignable

  void init(const std::shared_ptr<appfwk::ModuleConfiguration> mcfg) override;

private:

  // Commands
  void do_configure(const data_t& args);
  void get_reg(const data_t& args);
  void set_reg(const data_t& args);
  void get_bf(const data_t& args);
  void set_bf(const data_t& args);
  void gth_reset(const data_t& args);

  // Configuration
  const appmodel::FelixCardControllerModule* m_cfg;
  // FELIX Card
  std::map<uint32_t, std::shared_ptr<CardControllerWrapper> > m_card_wrappers;
};

} // namespace flxlibs
} // namespace dunedaq

#endif // FLXLIBS_PLUGINS_FELIXCARDCONTROLLER_HPP_
