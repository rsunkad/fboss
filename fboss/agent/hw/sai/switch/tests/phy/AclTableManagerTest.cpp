/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "fboss/agent/hw/sai/switch/SaiAclTableGroupManager.h"
#include "fboss/agent/hw/sai/switch/SaiAclTableManager.h"
#include "fboss/agent/hw/sai/switch/SaiSwitch.h"
#include "fboss/agent/hw/sai/switch/tests/ManagerTestBase.h"
#include "fboss/agent/types.h"

#include <string>

using namespace facebook::fboss;

namespace {
constexpr auto kAclTable2 = "AclTable2";
}

class AclTableManagerTest : public ManagerTestBase {
 public:
  int kPriority() const {
    return 1;
  }

  int kPriority2() const {
    return 2;
  }

  uint8_t kDscp() const {
    return 10;
  }

  folly::MacAddress kMac() const {
    return folly::MacAddress{"01:02:03:04:05:06"};
  }
  folly::MacAddress kMac2() {
    return folly::MacAddress{"00:02:03:04:05:06"};
  }

  cfg::AclActionType kActionType() const {
    return cfg::AclActionType::DENY;
  }
};

TEST_F(AclTableManagerTest, addAclTable) {
  auto aclTableId = saiManagerTable->aclTableManager()
                        .getAclTableHandle(SaiSwitch::kAclTable1)
                        ->aclTable->adapterKey();
  // Acl table is added as part of sai switch init in test setup
  auto stageGot = saiApiTable->aclApi().getAttribute(
      aclTableId, SaiAclTableTraits::Attributes::Stage());
  EXPECT_EQ(stageGot, SAI_ACL_STAGE_INGRESS);
  // Enabled fields
  EXPECT_TRUE(saiApiTable->aclApi().getAttribute(
      aclTableId, SaiAclTableTraits::Attributes::FieldDstMac{}));
  EXPECT_TRUE(saiApiTable->aclApi().getAttribute(
      aclTableId, SaiAclTableTraits::Attributes::FieldEthertype{}));
  auto bindPoints = saiApiTable->aclApi().getAttribute(
      aclTableId, SaiAclTableTraits::Attributes::BindPointTypeList());
  EXPECT_EQ(1, bindPoints.size());
  EXPECT_EQ(SAI_ACL_BIND_POINT_TYPE_PORT, *bindPoints.begin());
  // Check a few disabled fields
  EXPECT_FALSE(saiApiTable->aclApi().getAttribute(
      aclTableId, SaiAclTableTraits::Attributes::FieldSrcIpV6{}));
  EXPECT_FALSE(saiApiTable->aclApi().getAttribute(
      aclTableId, SaiAclTableTraits::Attributes::FieldL4DstPort{}));
}

TEST_F(AclTableManagerTest, addTwoAclTable) {
  // AclTable1 should already be added
  auto aclTableId = saiManagerTable->aclTableManager()
                        .getAclTableHandle(SaiSwitch::kAclTable1)
                        ->aclTable->adapterKey();
  auto table2 = std::make_shared<AclTable>(0, kAclTable2);
  AclTableSaiId aclTableId2 = saiManagerTable->aclTableManager().addAclTable(
      table2, SAI_ACL_STAGE_INGRESS);

  auto stageGot = saiApiTable->aclApi().getAttribute(
      aclTableId, SaiAclTableTraits::Attributes::Stage());
  EXPECT_EQ(stageGot, SAI_ACL_STAGE_INGRESS);

  auto stageGot2 = saiApiTable->aclApi().getAttribute(
      aclTableId2, SaiAclTableTraits::Attributes::Stage());
  EXPECT_EQ(stageGot2, SAI_ACL_STAGE_INGRESS);
}

TEST_F(AclTableManagerTest, addDupAclTable) {
  auto table1 = std::make_shared<AclTable>(0, SaiSwitch::kAclTable1);
  EXPECT_THROW(
      saiManagerTable->aclTableManager().addAclTable(
          table1, SAI_ACL_STAGE_INGRESS),
      FbossError);
}

TEST_F(AclTableManagerTest, getAclTable) {
  auto handle = saiManagerTable->aclTableManager().getAclTableHandle(
      SaiSwitch::kAclTable1);

  EXPECT_TRUE(handle);
  EXPECT_TRUE(handle->aclTable);
}

TEST_F(AclTableManagerTest, checkNonExistentAclTable) {
  auto handle =
      saiManagerTable->aclTableManager().getAclTableHandle(kAclTable2);

  EXPECT_FALSE(handle);
}

TEST_F(AclTableManagerTest, addAclEntryDscp) {
  auto aclTableId = saiManagerTable->aclTableManager()
                        .getAclTableHandle(SaiSwitch::kAclTable1)
                        ->aclTable->adapterKey();

  auto aclEntry = std::make_shared<AclEntry>(kPriority(), "AclEntry1");
  aclEntry->setDscp(kDscp());
  aclEntry->setActionType(kActionType());

  // DSCP not supported
  EXPECT_THROW(
      saiManagerTable->aclTableManager().addAclEntry(
          aclEntry, SaiSwitch::kAclTable1),
      FbossError);
}

TEST_F(AclTableManagerTest, addAclEntryDstMac) {
  auto aclTableId = saiManagerTable->aclTableManager()
                        .getAclTableHandle(SaiSwitch::kAclTable1)
                        ->aclTable->adapterKey();

  auto aclEntry = std::make_shared<AclEntry>(kPriority(), "AclEntry1");
  aclEntry->setDstMac(kMac());
  aclEntry->setActionType(kActionType());

  AclEntrySaiId aclEntryId = saiManagerTable->aclTableManager().addAclEntry(
      aclEntry, SaiSwitch::kAclTable1);

  auto tableIdGot = saiApiTable->aclApi().getAttribute(
      aclEntryId, SaiAclEntryTraits::Attributes::TableId());
  EXPECT_EQ(tableIdGot, aclTableId);
}

TEST_F(AclTableManagerTest, addAclEntryWithCounter) {
  auto aclTableId = saiManagerTable->aclTableManager()
                        .getAclTableHandle(SaiSwitch::kAclTable1)
                        ->aclTable->adapterKey();

  auto counter = cfg::TrafficCounter();
  counter.name_ref() = "stat0.c";
  MatchAction action = MatchAction();
  action.setTrafficCounter(counter);

  auto aclEntry = std::make_shared<AclEntry>(kPriority(), "AclEntry1");
  aclEntry->setDstMac(kMac());
  aclEntry->setAclAction(action);

  AclEntrySaiId aclEntryId = saiManagerTable->aclTableManager().addAclEntry(
      aclEntry, SaiSwitch::kAclTable1);

  auto tableIdGot = saiApiTable->aclApi().getAttribute(
      aclEntryId, SaiAclEntryTraits::Attributes::TableId());
  EXPECT_EQ(tableIdGot, aclTableId);

  auto aclCounterIdGot =
      saiApiTable->aclApi()
          .getAttribute(
              aclEntryId, SaiAclEntryTraits::Attributes::ActionCounter())
          .getData();

  auto tableIdGot2 = saiApiTable->aclApi().getAttribute(
      AclCounterSaiId(aclCounterIdGot),
      SaiAclCounterTraits::Attributes::TableId());
  EXPECT_EQ(tableIdGot2, aclTableId);
}

TEST_F(AclTableManagerTest, addTwoAclEntry) {
  auto aclTableId = saiManagerTable->aclTableManager()
                        .getAclTableHandle(SaiSwitch::kAclTable1)
                        ->aclTable->adapterKey();

  auto aclEntry = std::make_shared<AclEntry>(kPriority(), "AclEntry1");
  aclEntry->setDstMac(kMac());
  aclEntry->setActionType(kActionType());

  AclEntrySaiId aclEntryId = saiManagerTable->aclTableManager().addAclEntry(
      aclEntry, SaiSwitch::kAclTable1);

  auto tableIdGot = saiApiTable->aclApi().getAttribute(
      aclEntryId, SaiAclEntryTraits::Attributes::TableId());
  EXPECT_EQ(tableIdGot, aclTableId);

  auto aclEntry2 = std::make_shared<AclEntry>(kPriority2(), "AclEntry2");
  aclEntry->setDstMac(kMac2());
  aclEntry2->setActionType(kActionType());

  AclEntrySaiId aclEntryId2 = saiManagerTable->aclTableManager().addAclEntry(
      aclEntry2, SaiSwitch::kAclTable1);

  auto tableIdGot2 = saiApiTable->aclApi().getAttribute(
      aclEntryId2, SaiAclEntryTraits::Attributes::TableId());
  EXPECT_EQ(tableIdGot2, aclTableId);
}

TEST_F(AclTableManagerTest, addDupAclEntry) {
  auto aclEntry = std::make_shared<AclEntry>(kPriority(), "AclEntry1");
  aclEntry->setDstMac(kMac());
  aclEntry->setActionType(kActionType());

  saiManagerTable->aclTableManager().addAclEntry(
      aclEntry, SaiSwitch::kAclTable1);

  auto dupAclEntry = std::make_shared<AclEntry>(kPriority(), "AclEntry1");
  dupAclEntry->setDstMac(kMac2());
  dupAclEntry->setActionType(cfg::AclActionType::DENY);

  EXPECT_THROW(
      saiManagerTable->aclTableManager().addAclEntry(
          dupAclEntry, SaiSwitch::kAclTable1),
      FbossError);
}

TEST_F(AclTableManagerTest, getAclEntry) {
  auto aclEntry = std::make_shared<AclEntry>(kPriority(), "AclEntry1");
  aclEntry->setDstMac(kMac());
  aclEntry->setActionType(kActionType());

  saiManagerTable->aclTableManager().addAclEntry(
      aclEntry, SaiSwitch::kAclTable1);

  auto aclTableHandle = saiManagerTable->aclTableManager().getAclTableHandle(
      SaiSwitch::kAclTable1);

  EXPECT_TRUE(aclTableHandle);
  EXPECT_TRUE(aclTableHandle->aclTable);

  auto aclEntryHandle = saiManagerTable->aclTableManager().getAclEntryHandle(
      aclTableHandle, kPriority());

  EXPECT_TRUE(aclEntryHandle);
  EXPECT_TRUE(aclEntryHandle->aclEntry);
}

TEST_F(AclTableManagerTest, checkNonExistentAclEntry) {
  auto aclTableHandle = saiManagerTable->aclTableManager().getAclTableHandle(
      SaiSwitch::kAclTable1);

  EXPECT_TRUE(aclTableHandle);
  EXPECT_TRUE(aclTableHandle->aclTable);

  auto aclEntryHandle = saiManagerTable->aclTableManager().getAclEntryHandle(
      aclTableHandle, kPriority());
  EXPECT_FALSE(aclEntryHandle);
}
