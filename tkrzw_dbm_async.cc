/*************************************************************************************************
 * Asynchronous database manager adapter
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

#include "tkrzw_dbm.h"
#include "tkrzw_dbm_async.h"
#include "tkrzw_dbm_common_impl.h"
#include "tkrzw_str_util.h"
#include "tkrzw_thread_util.h"

namespace tkrzw {

AsyncDBM::AsyncDBM(DBM* dbm, int32_t num_worker_threads)
    : dbm_(dbm), queue_(), postproc_(nullptr) {
  assert(dbm != nullptr && num_worker_threads > 0);
  queue_.Start(num_worker_threads);
}

AsyncDBM::~AsyncDBM() {
  queue_.Stop(INT32MAX);
}

void AsyncDBM::SetCommonPostprocessor(std::unique_ptr<CommonPostprocessor> proc) {
  postproc_ = std::move(proc);
}

std::future<std::pair<Status, std::string>> AsyncDBM::Get(std::string_view key) {
  struct GetTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::string key;
    std::promise<std::pair<Status, std::string>> promise;
    void Do() override {
      std::string value;
      Status status = dbm->Get(key, &value);
      if (postproc != nullptr) {
        postproc->Postprocess("Get", status);
      }
      promise.set_value(std::make_pair(std::move(status), std::move(value)));
    }
  };
  auto task = std::make_unique<GetTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  task->key = key;
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<std::pair<Status, std::map<std::string, std::string>>> AsyncDBM::GetMulti(
    const std::vector<std::string_view>& keys) {
  struct GetMultiTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::vector<std::string> keys;
    std::vector<std::string_view> key_views;
    std::promise<std::pair<Status, std::map<std::string, std::string>>> promise;
    void Do() override {
      std::map<std::string, std::string> records;
      Status status = dbm->GetMulti(keys, &records);
      if (postproc != nullptr) {
        postproc->Postprocess("GetMulti", status);
      }
      promise.set_value(std::make_pair(std::move(status), std::move(records)));
    }
  };
  auto task = std::make_unique<GetMultiTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  task->keys.reserve(keys.size());
  for (const auto& key : keys) {
    task->keys.emplace_back(key);
  }
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<Status> AsyncDBM::Set(std::string_view key, std::string_view value, bool overwrite) {
  struct SetTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::string key;
    std::string value;
    bool overwrite;
    std::promise<Status> promise;
    void Do() override {
      Status status = dbm->Set(key, value, overwrite);
      if (postproc != nullptr) {
        postproc->Postprocess("Set", status);
      }
      promise.set_value(std::move(status));
    }
  };
  auto task = std::make_unique<SetTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  task->key = key;
  task->value = value;
  task->overwrite = overwrite;
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<Status> AsyncDBM::SetMulti(
    const std::map<std::string_view, std::string_view>& records, bool overwrite) {
  struct SetMultiTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::map<std::string, std::string> records;
    std::map<std::string_view, std::string_view> record_views;
    bool overwrite;
    std::promise<Status> promise;
    void Do() override {
      Status status = dbm->SetMulti(record_views, overwrite);
      if (postproc != nullptr) {
        postproc->Postprocess("SetMulti", status);
      }
      promise.set_value(std::move(status));
    }
  };
  auto task = std::make_unique<SetMultiTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  for (const auto& record : records) {
    task->records.emplace(std::make_pair(record.first, record.second));
  }
  for (const auto& record : task->records) {
    task->record_views.emplace(std::make_pair(
        std::string_view(record.first), std::string_view(record.second)));
  }
  task->overwrite = overwrite;
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<Status> AsyncDBM::Remove(std::string_view key) {
  struct RemoveTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::string key;
    std::promise<Status> promise;
    void Do() override {
      Status status = dbm->Remove(key);
      if (postproc != nullptr) {
        postproc->Postprocess("Remove", status);
      }
      promise.set_value(std::move(status));
    }
  };
  auto task = std::make_unique<RemoveTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  task->key = key;
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<Status> AsyncDBM::RemoveMulti(const std::vector<std::string_view>& keys) {
  struct RemoveMultiTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::vector<std::string> keys;
    std::vector<std::string_view> key_views;
    std::promise<Status> promise;
    void Do() override {
      Status status = dbm->RemoveMulti(key_views);
      if (postproc != nullptr) {
        postproc->Postprocess("RemoveMulti", status);
      }
      promise.set_value(std::move(status));
    }
  };
  auto task = std::make_unique<RemoveMultiTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  task->keys.reserve(keys.size());
  task->key_views.reserve(keys.size());
  for (const auto& key : keys) {
    task->keys.emplace_back(key);
    task->key_views.emplace_back(task->keys.back());
  }
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<Status> AsyncDBM::Append(
    std::string_view key, std::string_view value, std::string_view delim) {
  struct AppendTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::string key;
    std::string value;
    std::string delim;
    std::promise<Status> promise;
    void Do() override {
      Status status = dbm->Append(key, value, delim);
      if (postproc != nullptr) {
        postproc->Postprocess("Append", status);
      }
      promise.set_value(std::move(status));
    }
  };
  auto task = std::make_unique<AppendTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  task->key = key;
  task->value = value;
  task->delim = delim;
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<Status> AsyncDBM::AppendMulti(
    const std::map<std::string_view, std::string_view>& records, std::string_view delim) {
  struct AppendMultiTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::map<std::string, std::string> records;
    std::map<std::string_view, std::string_view> record_views;
    std::string delim;
    std::promise<Status> promise;
    void Do() override {
      Status status = dbm->AppendMulti(record_views, delim);
      if (postproc != nullptr) {
        postproc->Postprocess("AppendMulti", status);
      }
      promise.set_value(std::move(status));
    }
  };
  auto task = std::make_unique<AppendMultiTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  for (const auto& record : records) {
    task->records.emplace(std::make_pair(record.first, record.second));
  }
  for (const auto& record : task->records) {
    task->record_views.emplace(std::make_pair(
        std::string_view(record.first), std::string_view(record.second)));
  }
  task->delim = delim;
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<Status> AsyncDBM::CompareExchange(std::string_view key, std::string_view expected,
                                              std::string_view desired) {
  struct CompareExchangeTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::string key;
    std::string expected;
    std::string_view expected_view;
    std::string desired;
    std::string_view desired_view;
    std::promise<Status> promise;
    void Do() override {
      Status status = dbm->CompareExchange(key, expected_view, desired_view);
      if (postproc != nullptr) {
        postproc->Postprocess("CompareExchange", status);
      }
      promise.set_value(std::move(status));
    }
  };
  auto task = std::make_unique<CompareExchangeTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  task->key = key;
  if (expected.data() != nullptr) {
    task->expected = expected;
    task->expected_view = task->expected;
  }
  if (desired.data() != nullptr) {
    task->desired = desired;
    task->desired_view = task->desired;
  }
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<Status> AsyncDBM::CompareExchangeMulti(
    const std::vector<std::pair<std::string_view, std::string_view>>& expected,
    const std::vector<std::pair<std::string_view, std::string_view>>& desired) {
  struct CompareExchangeMultiTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::vector<std::string> placeholders;
    std::vector<std::pair<std::string_view, std::string_view>> expected;
    std::vector<std::pair<std::string_view, std::string_view>> desired;
    std::promise<Status> promise;
    void Do() override {
      Status status = dbm->CompareExchangeMulti(expected, desired);
      if (postproc != nullptr) {
        postproc->Postprocess("CompareExchangeMulti", status);
      }
      promise.set_value(std::move(status));
    }
  };
  auto task = std::make_unique<CompareExchangeMultiTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  task->placeholders.reserve(expected.size() * 2 + desired.size() * 2);
  for (const auto& record : expected) {
    task->placeholders.emplace_back(record.first);
    const std::string_view key = task->placeholders.back();
    if (record.second.data() == nullptr) {
      task->expected.emplace_back(std::make_pair(key, std::string_view()));
    } else {
      task->placeholders.emplace_back(record.second);
      task->expected.emplace_back(std::make_pair(
          key, std::string_view(task->placeholders.back())));
    }
  }
  for (const auto& record : desired) {
    task->placeholders.emplace_back(record.first);
    const std::string_view key = task->placeholders.back();
    if (record.second.data() == nullptr) {
      task->desired.emplace_back(std::make_pair(key, std::string_view()));
    } else {
      task->placeholders.emplace_back(record.second);
      task->desired.emplace_back(std::make_pair(
          key, std::string_view(task->placeholders.back())));
    }
  }
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<std::pair<Status, int64_t>> AsyncDBM::Increment(
    std::string_view key, int64_t increment, int64_t initial) {
  struct IncrementTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::string key;
    int64_t increment;
    int64_t initial;
    std::promise<std::pair<Status, int64_t>> promise;
    void Do() override {
      int64_t current = 0;
      Status status = dbm->Increment(key, increment, &current, initial);
      if (postproc != nullptr) {
        postproc->Postprocess("Increment", status);
      }
      promise.set_value(std::make_pair(std::move(status), current));
    }
  };
  auto task = std::make_unique<IncrementTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  task->key = key;
  task->increment = increment;
  task->initial = initial;
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<Status> AsyncDBM::Clear() {
  struct ClearTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::promise<Status> promise;
    void Do() override {
      Status status = dbm->Clear();
      if (postproc != nullptr) {
        postproc->Postprocess("Clear", status);
      }
      promise.set_value(std::move(status));
    }
  };
  auto task = std::make_unique<ClearTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<Status> AsyncDBM::Rebuild() {
  struct RebuildTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::promise<Status> promise;
    void Do() override {
      Status status = dbm->Rebuild();
      if (postproc != nullptr) {
        postproc->Postprocess("Rebuild", status);
      }
      promise.set_value(std::move(status));
    }
  };
  auto task = std::make_unique<RebuildTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<Status> AsyncDBM::Synchronize(bool hard, std::unique_ptr<DBM::FileProcessor> proc) {
  struct SynchronizeTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    bool hard;
    std::unique_ptr<DBM::FileProcessor> proc;
    std::promise<Status> promise;
    void Do() override {
      Status status = dbm->Synchronize(hard, proc.get());
      if (postproc != nullptr) {
        postproc->Postprocess("Synchronize", status);
      }
      promise.set_value(std::move(status));
    }
  };
  auto task = std::make_unique<SynchronizeTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  task->hard = hard;
  task->proc = std::move(proc);
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

std::future<std::pair<Status, std::vector<std::string>>> AsyncDBM::SearchModal(
    std::string_view mode, std::string_view pattern, size_t capacity) {
  struct SearchModalTask : public TaskQueue::Task {
    DBM* dbm;
    AsyncDBM::CommonPostprocessor* postproc;
    std::string mode;
    std::string pattern;
    size_t capacity;
    std::promise<std::pair<Status, std::vector<std::string>>> promise;
    void Do() override {
      std::vector<std::string> keys;
      Status status = SearchDBMModal(dbm, mode, pattern, &keys, capacity);
      if (postproc != nullptr) {
        postproc->Postprocess("SearchModal", status);
      }
      promise.set_value(std::move(std::make_pair(std::move(status), std::move(keys))));
    }
  };
  auto task = std::make_unique<SearchModalTask>();
  task->dbm = dbm_;
  task->postproc = postproc_.get();
  task->mode = mode;
  task->pattern = pattern;
  task->capacity = capacity;
  auto future = task->promise.get_future();
  queue_.Add(std::move(task));
  return future;
}

}  // namespace tkrzw

// END OF FILE