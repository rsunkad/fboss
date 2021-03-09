/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#pragma once

#include "fboss/agent/hw/sai/api/BridgeApi.h"
#include "fboss/agent/hw/sai/api/Types.h"
#include "fboss/agent/hw/sai/store/SaiObject.h"
#include "fboss/agent/types.h"

#include <memory>

namespace facebook::fboss {

class SaiManagerTable;
class SaiPlatform;

using SaiBridge = SaiObject<SaiBridgeTraits>;
using SaiBridgePort = SaiObject<SaiBridgePortTraits>;

struct SaiBridgeHandle {
  std::shared_ptr<SaiBridge> bridge;
};

class SaiBridgeManager {
 public:
  SaiBridgeManager(SaiManagerTable* managerTable, const SaiPlatform* platform);
  std::shared_ptr<SaiBridgePort> addBridgePort(
      SaiPortDescriptor portDescriptor,
      PortDescriptorSaiId saiId);

  sai_bridge_port_fdb_learning_mode_t getFdbLearningMode(
      cfg::L2LearningMode l2LearningMode) const;

  void setL2LearningMode(std::optional<cfg::L2LearningMode> l2LearningMode);

  cfg::L2LearningMode getL2LearningMode() const;

 private:
  SaiManagerTable* managerTable_;
  const SaiPlatform* platform_;
  std::unique_ptr<SaiBridgeHandle> bridgeHandle_;
  sai_bridge_port_fdb_learning_mode_t fdbLearningMode_{
      SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW};
};

} // namespace facebook::fboss
