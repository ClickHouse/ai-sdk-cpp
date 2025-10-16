/**
 * Embeddings Example - AI SDK C++
 *
 * This example demonstrates how to use the embeddings API with the AI SDK.
 * It shows how to:
 * - Generate embeddings for single and multiple texts
 * - Use different embedding models and dimensions
 * - Calculate cosine similarity between embeddings
 * - Handle errors and display results
 *
 * Usage:
 *   export OPENAI_API_KEY=your_key_here
 *   ./embeddings_example
 */

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <ai/openai.h>
#include <ai/types/embedding_options.h>

// Helper function to calculate cosine similarity between two embeddings
double cosine_similarity(const std::vector<double>& a,
                         const std::vector<double>& b) {
  if (a.size() != b.size()) {
    return 0.0;
  }

  double dot_product = 0.0;
  double norm_a = 0.0;
  double norm_b = 0.0;

  for (size_t i = 0; i < a.size(); ++i) {
    dot_product += a[i] * b[i];
    norm_a += a[i] * a[i];
    norm_b += b[i] * b[i];
  }

  if (norm_a == 0.0 || norm_b == 0.0) {
    return 0.0;
  }

  return dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

// Helper function to extract embedding as a vector of doubles
std::vector<double> extract_embedding(const nlohmann::json& data,
                                      size_t index) {
  std::vector<double> embedding;
  if (data.is_array() && index < data.size()) {
    for (const auto& val : data[index]["embedding"]) {
      embedding.push_back(val.get<double>());
    }
  }
  return embedding;
}

int main() {
  std::cout << "AI SDK C++ - Embeddings Example\n";
  std::cout << "================================\n\n";

  // Create OpenAI client
  auto client = ai::openai::create_client();
  if (!client.is_valid()) {
    std::cerr << "Error: Failed to create OpenAI client. Make sure "
                 "OPENAI_API_KEY is set.\n";
    return 1;
  }

  // Example 1: Basic single text embedding
  std::cout << "1. Single Text Embedding:\n";
  std::cout << "Text: \"Hello, world!\"\n\n";

  nlohmann::json input1 = "Hello, world!";
  ai::EmbeddingOptions options1("text-embedding-3-small", input1);
  auto result1 = client.embeddings(options1);

  if (result1) {
    auto embedding = result1.data[0]["embedding"];
    std::cout << "✓ Successfully generated embedding\n";
    std::cout << "  Dimensions: " << embedding.size() << "\n";
    std::cout << "  Token usage: " << result1.usage.total_tokens << " tokens\n";
    std::cout << "  First 5 values: [";
    for (size_t i = 0; i < std::min(size_t(5), embedding.size()); ++i) {
      std::cout << std::fixed << std::setprecision(6)
                << embedding[i].get<double>();
      if (i < 4)
        std::cout << ", ";
    }
    std::cout << ", ...]\n\n";
  } else {
    std::cout << "✗ Error: " << result1.error_message() << "\n\n";
  }

  // Example 2: Multiple texts embedding
  std::cout << "2. Multiple Texts Embedding:\n";
  nlohmann::json input2 = nlohmann::json::array(
      {"sunny day at the beach", "rainy afternoon in the city",
       "snowy night in the mountains"});

  ai::EmbeddingOptions options2("text-embedding-3-small", input2);
  auto result2 = client.embeddings(options2);

  if (result2) {
    std::cout << "✓ Successfully generated " << result2.data.size()
              << " embeddings\n";
    std::cout << "  Token usage: " << result2.usage.total_tokens << " tokens\n";
    for (size_t i = 0; i < result2.data.size(); ++i) {
      std::cout << "  Embedding " << i + 1
                << " dimensions: " << result2.data[i]["embedding"].size()
                << "\n";
    }
    std::cout << "\n";
  } else {
    std::cout << "✗ Error: " << result2.error_message() << "\n\n";
  }

  // Example 3: Embedding with custom dimensions
  std::cout << "3. Custom Dimensions (512 instead of default 1536):\n";
  nlohmann::json input3 = "Testing custom dimensions";
  ai::EmbeddingOptions options3("text-embedding-3-small", input3, 512);
  auto result3 = client.embeddings(options3);

  if (result3) {
    auto embedding = result3.data[0]["embedding"];
    std::cout << "✓ Successfully generated embedding with custom dimensions\n";
    std::cout << "  Dimensions: " << embedding.size() << " (requested: 512)\n";
    std::cout << "  Token usage: " << result3.usage.total_tokens
              << " tokens\n\n";
  } else {
    std::cout << "✗ Error: " << result3.error_message() << "\n\n";
  }

  // Example 4: Semantic similarity between texts
  std::cout << "4. Calculating Semantic Similarity:\n";
  nlohmann::json input4 = nlohmann::json::array(
      {"cat", "kitten", "dog", "puppy", "car", "automobile"});

  ai::EmbeddingOptions options4("text-embedding-3-small", input4);
  auto result4 = client.embeddings(options4);

  if (result4) {
    std::cout << "✓ Generated embeddings for similarity comparison\n\n";

    // Extract embeddings
    std::vector<std::string> texts = {"cat",   "kitten", "dog",
                                      "puppy", "car",    "automobile"};
    std::vector<std::vector<double>> embeddings;

    for (size_t i = 0; i < result4.data.size(); ++i) {
      embeddings.push_back(extract_embedding(result4.data, i));
    }

    // Calculate and display similarities
    std::cout << "  Similarity scores (cosine similarity):\n";
    std::cout << "  ----------------------------------------\n";
    std::cout << "  cat ↔ kitten:     " << std::fixed << std::setprecision(4)
              << cosine_similarity(embeddings[0], embeddings[1]) << "\n";
    std::cout << "  dog ↔ puppy:      " << std::fixed << std::setprecision(4)
              << cosine_similarity(embeddings[2], embeddings[3]) << "\n";
    std::cout << "  car ↔ automobile: " << std::fixed << std::setprecision(4)
              << cosine_similarity(embeddings[4], embeddings[5]) << "\n";
    std::cout << "  cat ↔ dog:        " << std::fixed << std::setprecision(4)
              << cosine_similarity(embeddings[0], embeddings[2]) << "\n";
    std::cout << "  cat ↔ car:        " << std::fixed << std::setprecision(4)
              << cosine_similarity(embeddings[0], embeddings[4]) << "\n\n";

    std::cout
        << "  Note: Similar concepts have similarity scores closer to 1.0\n\n";
  } else {
    std::cout << "✗ Error: " << result4.error_message() << "\n\n";
  }

  // Example 5: Using different embedding models
  std::cout << "5. Comparing Different Embedding Models:\n";
  nlohmann::json input5 = "Artificial intelligence and machine learning";

  // text-embedding-3-small
  ai::EmbeddingOptions options5a("text-embedding-3-small", input5);
  auto result5a = client.embeddings(options5a);

  if (result5a) {
    std::cout << "  text-embedding-3-small:\n";
    std::cout << "    Dimensions: " << result5a.data[0]["embedding"].size()
              << "\n";
    std::cout << "    Token usage: " << result5a.usage.total_tokens
              << " tokens\n";
  }

  // text-embedding-3-large
  ai::EmbeddingOptions options5b("text-embedding-3-large", input5);
  auto result5b = client.embeddings(options5b);

  if (result5b) {
    std::cout << "  text-embedding-3-large:\n";
    std::cout << "    Dimensions: " << result5b.data[0]["embedding"].size()
              << "\n";
    std::cout << "    Token usage: " << result5b.usage.total_tokens
              << " tokens\n";
  }

  std::cout << "\n";

  // Example 6: Practical use case - Finding similar items
  std::cout << "6. Practical Use Case - Finding Most Similar Item:\n";

  std::string query = "I need a programming language for web development";
  std::vector<std::string> documents = {
      "Python is great for data science and machine learning",
      "JavaScript is the language of the web and runs in browsers",
      "C++ is perfect for high-performance systems programming",
      "Java is widely used for enterprise applications",
      "TypeScript adds types to JavaScript for better development"};

  // Add query at the beginning
  nlohmann::json input6 = nlohmann::json::array();
  input6.push_back(query);
  for (const auto& doc : documents) {
    input6.push_back(doc);
  }

  ai::EmbeddingOptions options6("text-embedding-3-small", input6);
  auto result6 = client.embeddings(options6);

  if (result6) {
    std::cout << "  Query: \"" << query << "\"\n\n";
    std::cout << "  Similarity to documents:\n";
    std::cout << "  ----------------------------------------\n";

    // Extract query embedding
    auto query_embedding = extract_embedding(result6.data, 0);

    // Calculate similarity to each document
    std::vector<std::pair<size_t, double>> similarities;
    for (size_t i = 0; i < documents.size(); ++i) {
      auto doc_embedding = extract_embedding(result6.data, i + 1);
      double sim = cosine_similarity(query_embedding, doc_embedding);
      similarities.push_back({i, sim});
    }

    // Sort by similarity (highest first)
    std::sort(similarities.begin(), similarities.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Display results
    for (size_t i = 0; i < similarities.size(); ++i) {
      size_t idx = similarities[i].first;
      double sim = similarities[i].second;
      std::cout << "  " << (i + 1) << ". [" << std::fixed
                << std::setprecision(4) << sim << "] " << documents[idx]
                << "\n";
    }
    std::cout << "\n";
  } else {
    std::cout << "✗ Error: " << result6.error_message() << "\n\n";
  }

  // Example 7: Error handling
  std::cout << "7. Error Handling:\n";

  // Test with invalid model
  nlohmann::json input7 = "Test error handling";
  ai::EmbeddingOptions options7("invalid-model-name", input7);
  auto result7 = client.embeddings(options7);

  if (!result7) {
    std::cout << "✓ Error properly handled for invalid model:\n";
    std::cout << "  Error message: " << result7.error_message() << "\n\n";
  }

  std::cout << "\nExample completed!\n";
  std::cout << "\nTips:\n";
  std::cout
      << "  - text-embedding-3-small: 1536 dimensions, faster and cheaper\n";
  std::cout << "  - text-embedding-3-large: 3072 dimensions, higher quality\n";
  std::cout << "  - Use custom dimensions to reduce vector storage size\n";
  std::cout << "  - Cosine similarity scores closer to 1.0 indicate more "
               "similar texts\n";
  std::cout << "\nMake sure to set your API key:\n";
  std::cout << "  export OPENAI_API_KEY=your_openai_key\n";

  return 0;
}
