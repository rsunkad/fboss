/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/hw/sai/tracer/PortApiTracer.h"
#include "fboss/agent/hw/sai/tracer/Utils.h"

namespace facebook::fboss {

WRAP_CREATE_FUNC(
    router_interface,
    SAI_OBJECT_TYPE_ROUTER_INTERFACE,
    routerInterface);
WRAP_REMOVE_FUNC(
    router_interface,
    SAI_OBJECT_TYPE_ROUTER_INTERFACE,
    routerInterface);
WRAP_SET_ATTR_FUNC(
    router_interface,
    SAI_OBJECT_TYPE_ROUTER_INTERFACE,
    routerInterface);
WRAP_GET_ATTR_FUNC(
    router_interface,
    SAI_OBJECT_TYPE_ROUTER_INTERFACE,
    routerInterface);

sai_status_t wrap_get_router_interface_stats(
    sai_object_id_t router_interface_id,
    uint32_t number_of_counters,
    const sai_stat_id_t* counter_ids,
    uint64_t* counters) {
  return SaiTracer::getInstance()
      ->routerInterfaceApi_->get_router_interface_stats(
          router_interface_id, number_of_counters, counter_ids, counters);
}

sai_status_t wrap_get_router_interface_stats_ext(
    sai_object_id_t router_interface_id,
    uint32_t number_of_counters,
    const sai_stat_id_t* counter_ids,
    sai_stats_mode_t mode,
    uint64_t* counters) {
  return SaiTracer::getInstance()
      ->routerInterfaceApi_->get_router_interface_stats_ext(
          router_interface_id, number_of_counters, counter_ids, mode, counters);
}

sai_status_t wrap_clear_router_interface_stats(
    sai_object_id_t router_interface_id,
    uint32_t number_of_counters,
    const sai_stat_id_t* counter_ids) {
  return SaiTracer::getInstance()
      ->routerInterfaceApi_->clear_router_interface_stats(
          router_interface_id, number_of_counters, counter_ids);
}

sai_router_interface_api_t* wrappedRouterInterfaceApi() {
  static sai_router_interface_api_t routerInterfaceWrappers;

  routerInterfaceWrappers.create_router_interface =
      &wrap_create_router_interface;
  routerInterfaceWrappers.remove_router_interface =
      &wrap_remove_router_interface;
  routerInterfaceWrappers.set_router_interface_attribute =
      &wrap_set_router_interface_attribute;
  routerInterfaceWrappers.get_router_interface_attribute =
      &wrap_get_router_interface_attribute;
  routerInterfaceWrappers.get_router_interface_stats =
      &wrap_get_router_interface_stats;
  routerInterfaceWrappers.get_router_interface_stats_ext =
      &wrap_get_router_interface_stats_ext;
  routerInterfaceWrappers.clear_router_interface_stats =
      &wrap_clear_router_interface_stats;

  return &routerInterfaceWrappers;
}

void setRouterInterfaceAttributes(
    const sai_attribute_t* attr_list,
    uint32_t attr_count,
    std::vector<std::string>& attrLines) {
  for (int i = 0; i < attr_count; ++i) {
    switch (attr_list[i].id) {
      case SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS:
        macAddressAttr(attr_list, i, attrLines);
        break;
      case SAI_ROUTER_INTERFACE_ATTR_TYPE:
        attrLines.push_back(s32Attr(attr_list, i));
        break;
      case SAI_ROUTER_INTERFACE_ATTR_MTU:
        attrLines.push_back(u32Attr(attr_list, i));
        break;
      case SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID:
      case SAI_ROUTER_INTERFACE_ATTR_VLAN_ID:
        attrLines.push_back(oidAttr(attr_list, i));
        break;
      default:
        // TODO(zecheng): Better check for newly added attributes (T69350100)
        break;
    }
  }
}

} // namespace facebook::fboss
