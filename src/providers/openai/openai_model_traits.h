#pragma once

#include <optional>
#include <string_view>

namespace ai {
namespace openai {

/// Whether `model` belongs to the GPT-5 or GPT-6 family. These models are
/// reasoning-first and reject the sampling controls (`temperature`, `top_p`)
/// on both Chat Completions and the Responses API.
bool is_reasoning_family(std::string_view model);

/// The lowest reasoning effort a GPT-5 or GPT-6 family model accepts, or
/// nullopt when the parameter must be left out: the model is not in the
/// family, or it is a variant whose accepted values are fixed or not known.
/// An unsupported value fails the request with HTTP 400, so the field is only
/// sent where the value is known to be accepted. The same values apply to
/// `reasoning_effort` on Chat Completions and `reasoning.effort` on the
/// Responses API.
std::optional<std::string_view> lowest_reasoning_effort(std::string_view model);

}  // namespace openai
}  // namespace ai
