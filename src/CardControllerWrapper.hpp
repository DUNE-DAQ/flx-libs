/**
 * @file CardControllerWrapper.hpp
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef FLXLIBS_SRC_CARDCONTROLLERWRAPPER_HPP_
#define FLXLIBS_SRC_CARDCONTROLLERWRAPPER_HPP_

#include "flxcard/FlxCard.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <mutex>
#include <string>

namespace dunedaq {
namespace appmodel {
  class FelixInterface;
  class FelixDataSender;
}
namespace flxlibs {

class CardControllerWrapper
{
public:
  /**
   * @brief CardControllerWrapper Constructor
   */
  CardControllerWrapper(uint32_t device_id, const appmodel::FelixInterface * flx_cfg, const std::vector<const appmodel::FelixDataSender*>& flx_senders);
  ~CardControllerWrapper();
  CardControllerWrapper(const CardControllerWrapper&) = delete;            ///< Not copy-constructible
  CardControllerWrapper& operator=(const CardControllerWrapper&) = delete; ///< Not copy-assignable
  CardControllerWrapper(CardControllerWrapper&&) = delete;                 ///< Not move-constructible
  CardControllerWrapper& operator=(CardControllerWrapper&&) = delete;      ///< Not move-assignable

  using data_t = nlohmann::json;
  void init();
  void configure();

  uint64_t get_register(std::string key);             // NOLINT(build/unsigned)
  void set_register(std::string key, uint64_t value); // NOLINT(build/unsigned)
  uint64_t get_bitfield(std::string key);             // NOLINT(build/unsigned)
  void set_bitfield(std::string key, uint64_t value); // NOLINT(build/unsigned)
  void gth_reset();
  void check_alignment(uint64_t aligned);

private:

  // Card
  void open_card();
  void close_card();

  // Card object
  uint32_t m_device_id;


  using UniqueFlxCard = std::unique_ptr<FlxCard>;

  const appmodel::FelixInterface* m_flx_cfg;
  const std::vector<const appmodel::FelixDataSender*> m_flx_senders;

  UniqueFlxCard m_flx_card;
  std::mutex m_card_mutex;
};

} // namespace flxlibs
} // namespace dunedaq

#endif // FLXLIBS_SRC_CARDCONTROLLERWRAPPER_HPP_
