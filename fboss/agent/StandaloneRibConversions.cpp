/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/StandaloneRibConversions.h"

#include "fboss/agent/SwSwitchRouteUpdateWrapper.h"

#include "fboss/agent/SwSwitch.h"
#include "fboss/agent/rib/ForwardingInformationBaseUpdater.h"
#include "fboss/agent/rib/RoutingInformationBase.h"
#include "fboss/agent/state/ForwardingInformationBaseMap.h"
#include "fboss/agent/state/RouteTableMap.h"
#include "fboss/agent/state/SwitchState.h"

namespace facebook::fboss {

std::unique_ptr<rib::RoutingInformationBase> switchStateToStandaloneRib(
    const std::shared_ptr<RouteTableMap>& swStateRib) {
  auto serializedSwState = swStateRib->toFollyDynamic();
  folly::dynamic serialized = folly::dynamic::object;
  for (const auto& entry : serializedSwState[kEntries]) {
    serialized[folly::to<std::string>(entry[kRouterId].asInt())] = entry;
  }
  return rib::RoutingInformationBase::fromFollyDynamic(serialized);
}

std::shared_ptr<RouteTableMap> standaloneToSwitchStateRib(
    const rib::RoutingInformationBase& standaloneRib) {
  auto serializedRib = standaloneRib.toFollyDynamic();
  folly::dynamic serialized = folly::dynamic::object;
  serialized[kExtraFields] = folly::dynamic::object;
  serialized[kEntries] = folly::dynamic::array;
  for (const auto& entry : serializedRib.values()) {
    serialized[kEntries].push_back(entry);
  }
  return RouteTableMap::fromFollyDynamic(serialized);
}

void programRib(
    rib::RoutingInformationBase& standaloneRib,
    SwSwitch* swSwitch) {
  for (auto routerID : standaloneRib.getVrfList()) {
    standaloneRib.update(
        routerID,
        ClientID(-1),
        AdminDistance(-1),
        {},
        {},
        false,
        "post-warmboot FIB sync",
        &swSwitchFibUpdate,
        static_cast<void*>(swSwitch));
  }
}

std::shared_ptr<ForwardingInformationBaseMap> fibFromStandaloneRib(
    rib::RoutingInformationBase& rib) {
  auto state = std::make_shared<SwitchState>();
  auto fillInFib =
      [&state](
          facebook::fboss::RouterID vrf,
          const facebook::fboss::rib::IPv4NetworkToRouteMap& v4NetworkToRoute,
          const facebook::fboss::rib::IPv6NetworkToRouteMap& v6NetworkToRoute,
          void* cookie) {
        facebook::fboss::rib::ForwardingInformationBaseUpdater fibUpdater(
            vrf, v4NetworkToRoute, v6NetworkToRoute);
        fibUpdater(state);
      };

  for (auto routerID : rib.getVrfList()) {
    rib.update(
        routerID,
        ClientID(-1),
        AdminDistance(-1),
        {},
        {},
        false,
        "rib to fib",
        fillInFib,
        nullptr);
  }
  return state->getFibs();
}
} // namespace facebook::fboss
