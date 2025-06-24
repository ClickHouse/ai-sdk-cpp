#pragma once

#include "stream_event.h"

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <string>

namespace ai {

/// Forward declaration for StreamResult implementation
namespace internal {
class StreamResultImpl;
}

/// Result from streaming text generation
class StreamResult {
 public:
  /// Iterator for consuming stream events
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

  /// Constructor (implementation will be in source file)
  StreamResult();

  /// Destructor
  ~StreamResult();

  /// Move constructor
  StreamResult(StreamResult&& other) noexcept;

  /// Move assignment operator
  StreamResult& operator=(StreamResult&& other) noexcept;

  /// Delete copy constructor and assignment (streams are not copyable)
  StreamResult(const StreamResult&) = delete;
  StreamResult& operator=(const StreamResult&) = delete;

  /// Begin iterator
  iterator begin() const;

  /// End iterator
  iterator end() const;

  /// Process all events with a callback function
  void for_each(std::function<void(const StreamEvent&)> callback) const;

  /// Collect all text from the stream (blocks until completion)
  std::string collect_all() const;

  /// Check if stream has encountered an error
  bool has_error() const;

  /// Get error message if any
  std::string error_message() const;

  /// Check if stream is complete
  bool is_complete() const;

 private:
  std::unique_ptr<internal::StreamResultImpl> stream_result_impl_;
};

}  // namespace ai