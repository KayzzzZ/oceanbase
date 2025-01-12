/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#pragma once

#define USING_LOG_PREFIX RPC_TEST

#include <gtest/gtest.h>
#include <string>
#include "lib/hash/ob_array_hash_map.h"         // ObArrayHashMap
#include "ob_simple_log_server.h"

namespace oceanbase
{
namespace unittest
{

class ObSimpleLogClusterTestBase : public testing::Test
{
public:
  ObSimpleLogClusterTestBase()
  {
    SERVER_LOG(INFO, "ObSimpleLogClusterTestBase construct", K(member_cnt_), K(node_cnt_));
  }
  /*
  ObSimpleLogClusterTestBase(int64_t member_cnt = 3, int64_t node_cnt = 7)
  {
    member_cnt_ = member_cnt;
    node_cnt_ = node_cnt;
    SERVER_LOG(INFO, "ObSimpleLogClusterTestBase construct", K(member_cnt_), K(node_cnt_));
  }
  */
  virtual ~ObSimpleLogClusterTestBase() {}
  static int init();
  static int start();
  static int close();
  std::vector<ObSimpleLogServer*> &get_cluster() { return cluster_; }
  std::string &get_test_name() { return test_name_; }
  int64_t get_node_idx_base() { return node_idx_base_; }
  const ObMemberList &get_member_list() const {return member_list_;}
  const common::ObArrayHashMap<common::ObAddr, common::ObRegion> &get_member_region_map() const { return member_region_map_; }
  const ObMemberList &get_node_list() const {return node_list_;}
  int64_t get_node_cnt() const { return node_cnt_; }
  int64_t get_member_cnt() const { return member_cnt_; }

private:
  static int generate_sorted_server_list_(const int64_t node_cnt);

protected:
  static void SetUpTestCase();
  static void TearDownTestCase();

private:
  static std::vector<ObSimpleLogServer*> cluster_;
  static ObMemberList member_list_;
  static ObMemberList node_list_;
  static common::ObArrayHashMap<common::ObAddr, common::ObRegion> member_region_map_;
  static bool is_started_;
  static int64_t member_cnt_;
  static int64_t node_cnt_;
  static int64_t node_idx_base_;
  static std::string test_name_;
  //thread to deal signals
  static char sig_buf_[sizeof(ObSignalWorker) + sizeof(observer::ObSignalHandle)];
  static ObSignalWorker *sig_worker_;
  static observer::ObSignalHandle *signal_handle_;
};

} // end unittest
} // end oceanbase
