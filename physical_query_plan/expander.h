/*
 * Copyright [2012-2015] DaSE@ECNU
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * physical_query_plan/expander.h
 *
 *  Created on: Aug 27, 2013
 *      Author: wangli, fangzhuhe
 *       Email: fzhedu@gmail.com
 *
 * Description:
 */
#ifndef PHYSICAL_QUERY_PLAN_EXPANDER_H_
#define PHYSICAL_QUERY_PLAN_EXPANDER_H_
#include <pthread.h>
#include <map>
#include <vector>
#include <set>
#include "../physical_query_plan/BlockStreamIteratorBase.h"
#include "../../common/Schema/Schema.h"
#include "../../common/Block/BlockStreamBuffer.h"
#include "../../utility/ExpandabilityShrinkability.h"
#include "../../common/Logging.h"
#include "../../utility/lock.h"
#include "../../utility/ThreadPool.h"
#include "../../Environment.h"

#define EXPANDER_BUFFER_SIZE 1000

class BlockStreamExpander : public BlockStreamIteratorBase,
                            public ExpandabilityShrinkability {
 public:
  class State {
   public:
    friend class BlockStreamExpander;
    State(Schema* schema, BlockStreamIteratorBase* child, unsigned thread_count,
          unsigned block_size, unsigned block_count_in_buffer = 10);
    State(){};

   public:
    Schema* schema_;
    BlockStreamIteratorBase* child_;
    unsigned init_thread_count_;
    unsigned block_size_;
    unsigned block_count_in_buffer_;
    PartitionOffset partition_offset;

   private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version) {
      ar& schema_& child_& init_thread_count_& block_size_&
          block_count_in_buffer_;
    }
  };
  explicit BlockStreamExpander(State state);
  BlockStreamExpander();
  virtual ~BlockStreamExpander();
  bool Open(const PartitionOffset& partitoin_offset = 0);
  bool Next(BlockStreamBase* block);
  bool Close();
  void Print();

 private:
  static void* ExpandedWork(void* arg);
  bool ChildExhausted();
  bool CreateWorkingThread();
  void TerminateWorkingThread(pthread_t pid);
  bool Expand();
  bool Shrink();
  void AddIntoWorkingThreadList(pthread_t pid);
  bool RemoveFromWorkingThreadList(pthread_t pid);
  void AddIntoCalledBackThreadList(pthread_t pid);
  void RemoveFromCalledBackThreadList(pthread_t pid);
  unsigned GetDegreeOfParallelism();

 private:
  State state_;

  /*
   * The set of threads that are working normally.
   */
  std::set<pthread_t> in_work_expanded_thread_list_;
  pthread_t coordinate_pid_;
  ExpanderID expander_id_;
  Lock exclusive_expanding_;
  /*
   * The set of threads that have been called back but have not
   * finished the remaining work yet.
   */
  std::set<pthread_t> being_called_bacl_expanded_thread_list_;
  BlockStreamBuffer* block_stream_buffer_;
  volatile unsigned finished_thread_count_;
  volatile unsigned thread_count_;

  /*
   * whether at least one work thread has successfully finished!
   */
  volatile bool input_data_complete_;
  volatile bool one_thread_finished_;
  /*
   * this is a map storing the semaphore pointer for each thread to shrink.
   * the shrink() is blocked by sema until the thread is successful shrunk.
   */
  std::map<pthread_t, semaphore*> tid_to_shrink_semaphore_;
  Lock lock_;
  /*
   * The following code is for boost serialization.
   */
  unsigned long int received_tuples_;
  Logging* logging_;

 private:
  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<BlockStreamIteratorBase>(*this) &
        state_;
  }
};

#endif  // PHYSICAL_QUERY_PLAN_EXPANDER_H_
