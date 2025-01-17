/*************************************************************************************************
 * Tests for tkrzw_time_util.h
 *
 * Copyright 2020 Google LLC
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License.  You may obtain a copy of the License at
 *     https://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied.  See the License for the specific language governing permissions
 * and limitations under the License.
 *************************************************************************************************/

#include "tkrzw_sys_config.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "tkrzw_lib_common.h"
#include "tkrzw_str_util.h"
#include "tkrzw_time_util.h"

using namespace testing;

// Main routine
int main(int argc, char** argv) {
  InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(ThreadUtilTest, GetWallTime) {
  const double start_time = tkrzw::GetWallTime();
  EXPECT_GT(start_time, 0);
  std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int64_t>(1000)));
  const double end_time = tkrzw::GetWallTime();
  EXPECT_GT(end_time, start_time);
}

// END OF FILE
