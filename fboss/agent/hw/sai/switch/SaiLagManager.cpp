// Copyright 2004-present Facebook. All Rights Reserved.

#include "fboss/agent/hw/sai/switch/SaiLagManager.h"
#include "fboss/agent/hw/sai/switch/ConcurrentIndices.h"
#include "fboss/agent/hw/sai/switch/SaiManagerTable.h"
#include "fboss/agent/hw/sai/switch/SaiPortManager.h"
#include "fboss/agent/hw/sai/switch/SaiVlanManager.h"

#include "fboss/agent/hw/sai/store/SaiStore.h"

#include <folly/container/Enumerate.h>

namespace facebook::fboss {

LagSaiId SaiLagManager::addLag(
    const std::shared_ptr<AggregatePort>& aggregatePort) {
  XLOG(INFO) << "adding aggregate port : " << aggregatePort->getID();

  auto name = aggregatePort->getName();
  std::array<char, 32> labelValue{};
  for (auto i = 0; i < 32 && i < name.length(); i++) {
    labelValue[i] = name[i];
  }
  // TODO(pshaikh): support LAG without ports?

  auto& subPort = aggregatePort->sortedSubports().front();
  auto portSaiIdsIter = concurrentIndices_->portSaiIds.find(subPort.portID);
  // port must exist before LAG
  CHECK(portSaiIdsIter != concurrentIndices_->portSaiIds.end());
  auto portSaiId = portSaiIdsIter->second;
  // port must be part of some VLAN and all members of same LAG are part of same
  // VLAN
  auto vlanSaiIdsIter =
      concurrentIndices_->vlanIds.find(PortDescriptorSaiId(portSaiId));
  CHECK(vlanSaiIdsIter != concurrentIndices_->vlanIds.end());

  auto vlanID = vlanSaiIdsIter->second;

  SaiLagTraits::CreateAttributes createAttributes{labelValue, vlanID};

  auto& lagStore = saiStore_->get<SaiLagTraits>();
  auto lag = lagStore.setObject(
      SaiLagTraits::Attributes::Label{labelValue}, createAttributes);
  LagSaiId lagSaiId = lag->adapterKey();
  std::map<PortSaiId, std::shared_ptr<SaiLagMember>> members;
  for (auto iter : folly::enumerate(aggregatePort->subportAndFwdState())) {
    auto [subPort, fwdState] = *iter;
    auto member = addMember(lag, aggregatePort->getID(), subPort);
    setMemberState(member.second.get(), fwdState);
    members.emplace(std::move(member));
  }

  concurrentIndices_->vlanIds.emplace(
      PortDescriptorSaiId(lag->adapterKey()), vlanID);
  concurrentIndices_->aggregatePortIds.emplace(
      lag->adapterKey(), aggregatePort->getID());
  auto handle = std::make_unique<SaiLagHandle>();
  // create bridge port for LAG
  handle->bridgePort = managerTable_->bridgeManager().addBridgePort(
      SaiPortDescriptor(aggregatePort->getID()),
      PortDescriptorSaiId(lag->adapterKey()));

  handle->members = std::move(members);
  handle->lag = std::move(lag);
  handle->minimumLinkCount = aggregatePort->getMinimumLinkCount();
  handle->vlanId = vlanID;
  handles_.emplace(aggregatePort->getID(), std::move(handle));
  managerTable_->vlanManager().createVlanMember(
      vlanID, SaiPortDescriptor(aggregatePort->getID()));

  return lagSaiId;
}

void SaiLagManager::removeLag(
    const std::shared_ptr<AggregatePort>& aggregatePort) {
  XLOG(INFO) << "removing aggregate port : " << aggregatePort->getID();
  auto iter = handles_.find(aggregatePort->getID());
  if (iter == handles_.end()) {
    throw FbossError(
        "attempting to non-existing remove LAG ", aggregatePort->getID());
  }
  removeLagHandle(iter->first, iter->second.get());
  handles_.erase(iter->first);
}

void SaiLagManager::changeLag(
    const std::shared_ptr<AggregatePort>& oldAggregatePort,
    const std::shared_ptr<AggregatePort>& newAggregatePort) {
  auto handleIter = handles_.find(oldAggregatePort->getID());
  CHECK(handleIter != handles_.end());
  auto& saiLagHandle = handleIter->second;

  saiLagHandle->minimumLinkCount = newAggregatePort->getMinimumLinkCount();

  auto oldPortAndFwdState = oldAggregatePort->subportAndFwdState();
  auto newPortAndFwdState = newAggregatePort->subportAndFwdState();
  auto oldIter = oldPortAndFwdState.begin();
  auto newIter = newPortAndFwdState.begin();

  while (oldIter != oldPortAndFwdState.end() &&
         newIter != newPortAndFwdState.end()) {
    if (oldIter->first < newIter->first) {
      removeMember(oldAggregatePort->getID(), oldIter->first);
      oldIter++;
    } else if (newIter->first < oldIter->first) {
      // add member
      auto member = addMember(
          saiLagHandle->lag, newAggregatePort->getID(), newIter->first);
      setMemberState(member.second.get(), newIter->second);
      saiLagHandle->members.emplace(std::move(member));
      newIter++;
    } else if (oldIter->second != newIter->second) {
      // forwarding state changed
      auto member = getMember(saiLagHandle.get(), newIter->first);
      setMemberState(member, newIter->second);
      newIter++;
      oldIter++;
    }
    while (oldIter != oldPortAndFwdState.end()) {
      removeMember(oldAggregatePort->getID(), oldIter->first);
      oldIter++;
    }
    while (newIter != newPortAndFwdState.end()) {
      auto member = addMember(
          saiLagHandle->lag, newAggregatePort->getID(), newIter->first);
      setMemberState(member.second.get(), newIter->second);
      saiLagHandle->members.emplace(std::move(member));
      newIter++;
    }
  }
}

std::pair<PortSaiId, std::shared_ptr<SaiLagMember>> SaiLagManager::addMember(
    const std::shared_ptr<SaiLag>& lag,
    AggregatePortID aggregatePortID,
    PortID subPort) {
  auto portHandle = managerTable_->portManager().getPortHandle(subPort);
  CHECK(portHandle);
  portHandle->bridgePort.reset();
  auto saiPortId = portHandle->port->adapterKey();
  auto saiLagId = lag->adapterKey();

  SaiLagMemberTraits::AdapterHostKey adapterHostKey{saiLagId, saiPortId};
  SaiLagMemberTraits::CreateAttributes attrs{
      saiLagId, saiPortId, SaiLagMemberTraits::Attributes::EgressDisable{true}};
  auto& lagMemberStore = saiStore_->get<SaiLagMemberTraits>();
  auto member = lagMemberStore.setObject(adapterHostKey, attrs);
  concurrentIndices_->memberPort2AggregatePortIds.emplace(
      saiPortId, aggregatePortID);
  return {saiPortId, member};
}

void SaiLagManager::removeMember(AggregatePortID aggPort, PortID subPort) {
  auto handlesIter = handles_.find(aggPort);
  CHECK(handlesIter != handles_.end());
  auto portHandle = managerTable_->portManager().getPortHandle(subPort);
  CHECK(portHandle);
  auto saiPortId = portHandle->port->adapterKey();
  auto membersIter = handlesIter->second->members.find(saiPortId);
  if (membersIter == handlesIter->second->members.end()) {
    // link down will remove lag member, resulting in LACP machine processing
    // lag shrink. this  will also cause LACP machine to issue state delta to
    // remove  lag member. so  ignore the lag member  removal which  could be
    // issued second time by sw switch.
    XLOG(DBG6) << "member " << subPort << " of aggregate port " << aggPort
               << " was already removed.";
    return;
  }
  membersIter->second.reset();
  handlesIter->second->members.erase(membersIter);
  concurrentIndices_->memberPort2AggregatePortIds.erase(saiPortId);
  portHandle->bridgePort = managerTable_->bridgeManager().addBridgePort(
      SaiPortDescriptor(subPort),
      PortDescriptorSaiId(portHandle->port->adapterKey()));
}

SaiLagHandle* FOLLY_NULLABLE
SaiLagManager::getLagHandleIf(AggregatePortID aggregatePortID) const {
  auto iter = handles_.find(aggregatePortID);
  if (iter == handles_.end()) {
    return nullptr;
  }
  return iter->second.get();
}

SaiLagHandle* SaiLagManager::getLagHandle(
    AggregatePortID aggregatePortID) const {
  auto* handle = getLagHandleIf(aggregatePortID);
  if (handle) {
    return handle;
  }
  throw FbossError("handle for aggregate port ", aggregatePortID, " not found");
}

bool SaiLagManager::isMinimumLinkMet(AggregatePortID aggregatePortID) const {
  const auto* handle = getLagHandle(aggregatePortID);
  return handle->minimumLinkCount <= getActiveMemberCount(aggregatePortID);
}

void SaiLagManager::removeLagHandle(
    AggregatePortID aggPort,
    SaiLagHandle* handle) {
  // remove members
  while (!handle->members.empty()) {
    auto membersIter = handle->members.begin();
    auto portSaiId = membersIter->first;
    auto iter = concurrentIndices_->portIds.find(portSaiId);
    CHECK(iter != concurrentIndices_->portIds.end());
    removeMember(aggPort, iter->second);
  }
  // remove bridge port
  handle->bridgePort.reset();
  managerTable_->vlanManager().removeVlanMember(
      handle->vlanId, SaiPortDescriptor(aggPort));
  concurrentIndices_->vlanIds.erase(
      PortDescriptorSaiId(handle->lag->adapterKey()));
  concurrentIndices_->aggregatePortIds.erase(handle->lag->adapterKey());
  // remove lag
  handle->lag.reset();
}

SaiLagManager::~SaiLagManager() {
  for (auto& handlesIter : handles_) {
    auto& [aggPortID, handle] = handlesIter;
    removeLagHandle(aggPortID, handle.get());
  }
}

size_t SaiLagManager::getLagMemberCount(AggregatePortID aggPort) const {
  auto handle = getLagHandle(aggPort);
  return handle->members.size();
}

size_t SaiLagManager::getActiveMemberCount(AggregatePortID aggPort) const {
  const auto* handle = getLagHandle(aggPort);
  return std::count_if(
      std::begin(handle->members),
      std::end(handle->members),
      [](const auto& entry) {
        auto member = entry.second;
        auto attributes = member->attributes();
        auto isEgressDisabled =
            std::get<SaiLagMemberTraits::Attributes::EgressDisable>(attributes)
                .value();
        return !isEgressDisabled;
      });
}

void SaiLagManager::setMemberState(
    SaiLagMember* member,
    AggregatePort::Forwarding fwdState) {
  switch (fwdState) {
    case AggregatePort::Forwarding::DISABLED:
      member->setAttribute(SaiLagMemberTraits::Attributes::EgressDisable{true});
      break;
    case AggregatePort::Forwarding::ENABLED:
      member->setAttribute(
          SaiLagMemberTraits::Attributes::EgressDisable{false});
      break;
  }
}

SaiLagMember* SaiLagManager::getMember(SaiLagHandle* handle, PortID port) {
  auto portsIter = concurrentIndices_->portSaiIds.find(port);
  if (portsIter == concurrentIndices_->portSaiIds.end()) {
    throw FbossError("port sai id not found for lag member port ", port);
  }
  auto membersIter = handle->members.find(portsIter->second);
  if (membersIter == handle->members.end()) {
    throw FbossError("member not found for lag member port ", port);
  }
  return membersIter->second.get();
}

void SaiLagManager::disableMember(AggregatePortID aggPort, PortID subPort) {
  auto handleIter = handles_.find(aggPort);
  CHECK(handleIter != handles_.end());
  auto& saiLagHandle = handleIter->second;
  auto member = getMember(saiLagHandle.get(), subPort);
  setMemberState(member, AggregatePort::Forwarding::DISABLED);
}
} // namespace facebook::fboss
