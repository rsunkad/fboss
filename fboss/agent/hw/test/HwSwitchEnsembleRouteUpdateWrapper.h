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

#include "fboss/agent/RouteUpdateWrapper.h"
#include "fboss/agent/rib/RoutingInformationBase.h"
#include "fboss/agent/test/RouteDistributionGenerator.h"

namespace facebook::fboss {

class HwSwitchEnsemble;

void hwSwitchFibUpdate(
    facebook::fboss::RouterID vrf,
    const facebook::fboss::IPv4NetworkToRouteMap& v4NetworkToRoute,
    const facebook::fboss::IPv6NetworkToRouteMap& v6NetworkToRoute,
    void* cookie);

class HwSwitchEnsembleRouteUpdateWrapper : public RouteUpdateWrapper {
 public:
  explicit HwSwitchEnsembleRouteUpdateWrapper(
      HwSwitchEnsemble* hwEnsemble,
      RoutingInformationBase* rib);

  void programRoutes(
      RouterID rid,
      ClientID client,
      const utility::RouteDistributionGenerator::ThriftRouteChunks&
          routeChunks) {
    programRoutesImpl(rid, client, routeChunks, true /* add*/);
  }

  void unprogramRoutes(
      RouterID rid,
      ClientID client,
      const utility::RouteDistributionGenerator::ThriftRouteChunks&
          routeChunks) {
    programRoutesImpl(rid, client, routeChunks, false /* del*/);
  }

 private:
  void programRoutesImpl(
      RouterID rid,
      ClientID client,
      const utility::RouteDistributionGenerator::ThriftRouteChunks& routeChunks,
      bool add);
  void updateStats(
      const RoutingInformationBase::UpdateStatistics& /*stats*/) override {}
  AdminDistance clientIdToAdminDistance(ClientID clientId) const override;
  void programLegacyRib(const SyncFibFor& syncFibFor) override;
  void programClassIDLegacyRib(
      RouterID rid,
      const std::vector<folly::CIDRNetwork>& prefixes,
      std::optional<cfg::AclLookupClass> classId,
      bool async) override;

  HwSwitchEnsemble* hwEnsemble_;
};
} // namespace facebook::fboss
