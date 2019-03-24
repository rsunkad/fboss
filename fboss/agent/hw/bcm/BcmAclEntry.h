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

#include "fboss/agent/FbossError.h"
#include "fboss/agent/hw/bcm/BcmAclStat.h"
#include "fboss/agent/hw/bcm/types.h"

namespace facebook { namespace fboss {

class BcmSwitch;
class AclEntry;
enum class MirrorAction;
enum class MirrorDirection;

/**
 * BcmAclEntry is the class to abstract an acl's resources in BcmSwitch
 */
class BcmAclEntry {
public:
  BcmAclEntry(BcmSwitch* hw, int gid, const std::shared_ptr<AclEntry>& acl);
  ~BcmAclEntry();
  BcmAclEntryHandle getHandle() const {
    return handle_;
  }

  /**
   * Check whether the acl details of handle in h/w matches the s/w acl and
   * ranges
   */
  static bool isStateSame(BcmSwitch* hw, int gid, BcmAclEntryHandle handle,
                          const std::shared_ptr<AclEntry>& acl);
  folly::Optional<std::string> getIngressAclMirror();
  folly::Optional<std::string> getEgressAclMirror();

  void applyMirrorAction(
      const std::string& mirrorName,
      MirrorAction action,
      MirrorDirection direction);

 private:
  void createNewAclEntry();
  void createAclQualifiers();
  void createAclActions();
  void createAclStat();

  BcmSwitch* hw_;
  int gid_;
  std::shared_ptr<AclEntry> acl_;
  BcmAclEntryHandle handle_;
};

}} // facebook::fboss
