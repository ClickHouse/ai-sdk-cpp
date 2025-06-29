#pragma once

#include "stream_event.h"

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <string>

namespace ai {

namespace internal {
class StreamResultImpl {
 public:
  virtual ~StreamResultImpl() = default;
  virtual StreamEvent get_next_event() = 0;
  virtual bool has_more_events() const = 0;
  virtual void stop_stream() = 0;
};
}  // namespace internal

class StreamResult {
 public:
  class iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = StreamEvent;
    using difference_type = std::ptrdiff_t;
    using pointer = const StreamEvent*;
    using reference = const StreamEvent&;

    explicit iterator(const StreamResult* stream, bool end = false);

    reference operator*() const { return current_event_; }
    pointer operator->() const { return &current_event_; }
    iterator& operator++();
    iterator operator++(int);
    bool operator==(const iterator& other) const;
    bool operator!=(const iterator& other) const;

   private:
    const StreamResult* stream_;
    StreamEvent current_event_;
    bool is_end_;

    void advance();
  };

  StreamResult();

  explicit StreamResult(std::unique_ptr<internal::StreamResultImpl> impl);

  ~StreamResult();

  StreamResult(StreamResult&& other) noexcept;

  StreamResult& operator=(StreamResult&& other) noexcept;

  StreamResult(const StreamResult&) = delete;
  StreamResult& operator=(const StreamResult&) = delete;

  iterator begin() const;

  iterator end() const;

  void for_each(std::function<void(const StreamEvent&)> callback) const;

  std::string collect_all() const;

  bool has_error() const;

  std::string error_message() const;

  bool is_complete() const;

 private:
  std::unique_ptr<internal::StreamResultImpl> stream_result_impl_;
};

}  // namespace ai