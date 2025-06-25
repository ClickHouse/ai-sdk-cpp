#include "ai/types/stream_result.h"

namespace ai {

// StreamResult iterator implementation
StreamResult::iterator::iterator(const StreamResult* stream, bool end)
    : stream_(stream), current_event_(kStreamEventTypeFinish), is_end_(end) {
  if (!is_end_) {
    advance();
  }
}

void StreamResult::iterator::advance() {
  if (stream_->stream_result_impl_ &&
      stream_->stream_result_impl_->has_more_events()) {
    current_event_ = stream_->stream_result_impl_->get_next_event();
  } else {
    is_end_ = true;
  }
}

StreamResult::iterator& StreamResult::iterator::operator++() {
  advance();
  return *this;
}

StreamResult::iterator StreamResult::iterator::operator++(int) {
  iterator tmp = *this;
  advance();
  return tmp;
}

bool StreamResult::iterator::operator==(const iterator& other) const {
  return is_end_ == other.is_end_;
}

bool StreamResult::iterator::operator!=(const iterator& other) const {
  return !(*this == other);
}

// StreamResult implementation
StreamResult::StreamResult() = default;

StreamResult::StreamResult(std::unique_ptr<internal::StreamResultImpl> impl)
    : stream_result_impl_(std::move(impl)) {}

StreamResult::~StreamResult() {
  if (stream_result_impl_) {
    stream_result_impl_->stop_stream();
  }
}

StreamResult::StreamResult(StreamResult&& other) noexcept = default;
StreamResult& StreamResult::operator=(StreamResult&& other) noexcept = default;

StreamResult::iterator StreamResult::begin() const {
  return iterator(this, false);
}

StreamResult::iterator StreamResult::end() const {
  return iterator(this, true);
}

void StreamResult::for_each(
    std::function<void(const StreamEvent&)> callback) const {
  for (const auto& event : *this) {
    callback(event);
  }
}

std::string StreamResult::collect_all() const {
  std::string result;
  for (const auto& event : *this) {
    if (event.is_text_delta()) {
      result += event.text_delta;
    }
  }
  return result;
}

bool StreamResult::has_error() const {
  // Check if any event was an error
  for (const auto& event : *this) {
    if (event.is_error()) {
      return true;
    }
  }
  return false;
}

std::string StreamResult::error_message() const {
  for (const auto& event : *this) {
    if (event.is_error() && event.error.has_value()) {
      return event.error.value();
    }
  }
  return "";
}

bool StreamResult::is_complete() const {
  return !stream_result_impl_ || !stream_result_impl_->has_more_events();
}

}  // namespace ai