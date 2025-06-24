#pragma once

#include "enums.h"
#include "generate_options.h"

#include <functional>
#include <memory>

namespace ai {

/// Options for streaming text generation (inherits from GenerateOptions)
struct StreamOptions : public GenerateOptions {
  /// Callback for handling streaming text chunks
  std::function<void(const std::string&)> on_text_chunk;

  /// Callback for handling stream completion
  std::function<void(const GenerateResult&)> on_complete;

  /// Callback for handling errors during streaming
  std::function<void(const std::string&)> on_error;

  /// Constructor with model and prompt
  StreamOptions(std::string model_name, std::string user_prompt)
      : GenerateOptions(std::move(model_name), std::move(user_prompt)) {}

  /// Constructor with model, system, and prompt
  StreamOptions(std::string model_name,
                std::string system_prompt,
                std::string user_prompt)
      : GenerateOptions(std::move(model_name),
                        std::move(system_prompt),
                        std::move(user_prompt)) {}

  /// Constructor with model and messages
  StreamOptions(std::string model_name, Messages conversation)
      : GenerateOptions(std::move(model_name), std::move(conversation)) {}

  /// Default constructor
  StreamOptions() = default;
};

/// Event in a text generation stream
struct StreamEvent {
  StreamEventType type;              ///< Type of stream event
  std::string text_delta;            ///< Text chunk (for TextDelta events)
  std::optional<std::string> error;  ///< Error message (for Error events)

  /// Constructor for text delta events
  StreamEvent(std::string text)
      : type(kStreamEventTypeTextDelta), text_delta(std::move(text)) {}

  /// Constructor for error events
  StreamEvent(StreamEventType event_type, std::string error_msg)
      : type(event_type), error(std::move(error_msg)) {}

  /// Constructor for other event types
  explicit StreamEvent(StreamEventType event_type) : type(event_type) {}

  /// Check if this is a text delta event
  bool isTextDelta() const { return type == kStreamEventTypeTextDelta; }

  /// Check if this is an error event
  bool isError() const { return type == kStreamEventTypeError; }

  /// Check if this is a finish event
  bool isFinish() const { return type == kStreamEventTypeFinish; }
};

/// Forward declaration for StreamResult implementation
namespace internal {
class StreamResultImpl;
}

/// Result from streaming text generation
class StreamResult {
 private:
  std::unique_ptr<internal::StreamResultImpl> pimpl;

 public:
  /// Iterator for consuming stream events
  class iterator {
   private:
    StreamResult* stream_;
    StreamEvent current_event_;
    bool is_end_;

   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = StreamEvent;
    using difference_type = std::ptrdiff_t;
    using pointer = const StreamEvent*;
    using reference = const StreamEvent&;

    iterator(StreamResult* stream, bool end = false);

    reference operator*() const { return current_event_; }
    pointer operator->() const { return &current_event_; }
    iterator& operator++();
    iterator operator++(int);
    bool operator==(const iterator& other) const;
    bool operator!=(const iterator& other) const;

   private:
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
  iterator begin();

  /// End iterator
  iterator end();

  /// Process all events with a callback function
  void for_each(std::function<void(const StreamEvent&)> callback);

  /// Collect all text from the stream (blocks until completion)
  std::string collect_all();

  /// Check if stream has encountered an error
  bool has_error() const;

  /// Get error message if any
  std::string error_message() const;

  /// Check if stream is complete
  bool is_complete() const;
};

}  // namespace ai