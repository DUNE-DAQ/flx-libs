/**
 * @file FelixIssues.hpp FELIX card related ERS issues
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef FLXLIBS_SRC_FELIXISSUES_HPP_
#define FLXLIBS_SRC_FELIXISSUES_HPP_

#include <ers/Issue.hpp>
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.

#include <string>

namespace dunedaq {

ERS_DECLARE_ISSUE(flxlibs, CardError, " FELIX Card Internal Error: " << intererror, ((std::string)intererror))

ERS_DECLARE_ISSUE(flxlibs, InitializationError, " FELIX Initialization Error: " << initerror, ((std::string)initerror))

ERS_DECLARE_ISSUE(flxlibs, ConfigurationError, " FELIX Configuration Error: " << conferror, ((std::string)conferror))

ERS_DECLARE_ISSUE(flxlibs, QueueTimeoutError, " FELIX queue timed out: " << queuename, ((std::string)queuename))

ERS_DECLARE_ISSUE(flxlibs, ChannelAlignment, " Channel not aligned: " << channel, ((int)channel)) // NOLINT

ERS_DECLARE_ISSUE(flxlibs, UnexpectedChunk, " Unexpected chunk size: " << chunksize << " (observed) != " << expected << " (expected)",
                  ((int)chunksize)((size_t)expected)) // NOLINT

ERS_DECLARE_ISSUE(flxlibs,
                  ParserOperationQueuePushFailure,
                  " ParserOps couldn't push to queue! Failed chunk: " << chunk,
                  ((std::string)chunk))

ERS_DECLARE_ISSUE(flxlibs,
                  ElinkConfigurationInconsistency,
                  " Inconsistent number of ELinks requested. Num links: " << num_links,
                  ((int)num_links)) // NOLINT

ERS_DECLARE_ISSUE(flxlibs,
                  BlockSizeConfigurationInconsistency,
                  " Invalid FELIX block size and 32b trailer configuration requested: " << block_size,
                  ((int)block_size)) // NOLINT

ERS_DECLARE_ISSUE_BASE(flxlibs,
                       ResourceQueueError,
                       flxlibs::ConfigurationError,
                       " The " << queueType << " queue was not successfully created. ",
                       ((std::string)name),
                       ((std::string)queueType))

} // namespace dunedaq

#endif // FLXLIBS_SRC_FELIXISSUES_HPP_
