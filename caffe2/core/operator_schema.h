#ifndef CAFFE2_CORE_OPERATOR_SCHEMA_H_
#define CAFFE2_CORE_OPERATOR_SCHEMA_H_

#include <climits>
#include <functional>
#include <initializer_list>
#include <set>
#include <vector>

#include "caffe2/core/common.h"
#include "caffe2/core/registry.h"
#include "caffe2/proto/caffe2.pb.h"

namespace caffe2 {

// A const value returned by OpSchema::CalculateOutput() if the number of
// output cannot be determined.
constexpr int kCannotComputeNumOutputs = -1;

/**
 * @brief A class to record the schema of an op.
 *
 * OpSchema records the common interface of an op specified by its name. This
 * is optional for each operator implemented in Caffe2 but is strongly
 * recommended.
 *
 * To register an OpSchema, one can use the macro OPERATOR_SCHEMA(name) and
 * then append the various functions in the class. For example, for an op
 * that itakes in two inputs, one output, and the first input and output
 * could be in-place, can be written as
 *
 *     OPERATOR_SCHEMA(name)
 *         .NumInputs(2).NumOutputs(1).AllowInplace({{0, 0}});
 */
class OpSchema {
 public:
  OpSchema() : file_("unknown"), line_(0) { Init(); }
  OpSchema(const string& file, const int line)
      : file_(file), line_(line) { Init(); }

  /**
   * @brief Returns the file that the op schema is registered from.
   */
  inline const string& file() const { return file_; }

  /**
   * @brief Returns the line in file that the op schema is registered from.
   */
  inline int line() const { return line_; }

  /**
   * @brief Verifies if an operator definition protobuf matches the pattern
   * specified in the schema.
   */
  bool Verify(const OperatorDef& def) const;

  // Functions to set the property of the operator schemas.
  // Sets the number of inputs, either a fixed number or a min and a max.

  /**
   * @brief A single input.
   */
  OpSchema& NumInputs(int n);
  /**
   * @brief Input could be in range [min, max], inclusive.
   */
  OpSchema& NumInputs(int min, int max);
  /**
   * @brief Input could be one of the values specified in allowed_input_nums.
   */
  OpSchema& NumInputs(set<int> allowed_input_nums);
  /**
   * @brief Input is checked with a specified function.
   */
  OpSchema& NumInputs(std::function<bool(int)> func);

  // Sets the number of outputs, either a fixed number, a min and a max,
  // or a function that takes in the input number and produces an output
  // number. Use only one function in the set below.
  /**
   * @brief A single output.
   */
  OpSchema& NumOutputs(int n);
  /**
   * @brief Output could be in range [min, max], inclusive.
   */
  OpSchema& NumOutputs(int min, int max);
  /**
   * @brief Output could be one of the values specified in allowed_output_nums.
   */
  OpSchema& NumOutputs(set<int> allowed_output_nums);
  /**
   * @brief Output is checked with a specified function.
   */
  OpSchema& NumOutputs(std::function<bool(int)> func);

  // Set the function that can calculate the number of output based on the
  // number of input. Use only one function in the set below.
  /**
   * @brief Set the output calculator to a user-defined function.
   */
  OpSchema& OutputCalculator(std::function<int(int)> calc);
  /**
   * @brief Set the number of outputs to be the same as the number of inputs.
   */
  OpSchema& SameNumberOfOutput();

  // Sets the rule to allow optional in-place operation.
  OpSchema& AllowInplace(std::function<bool(int, int)> inplace);
  OpSchema& AllowInplace(set<std::pair<int, int>> inplace);
  OpSchema& AllowOneToOneInplace();
  // Sets the rule to enforce in-place opeartion.
  OpSchema& EnforceInplace(std::function<bool(int, int)> inplace);
  OpSchema& EnforceInplace(set<std::pair<int, int>> inplace);
  OpSchema& EnforceOneToOneInplace();

  /**
   * @brief A function to allow one to get the number of outputs based on the
   * number of inputs, if this schema supports it.
   */
  int CalculateOutput(int num_input) const;

 private:
  void Init() {
    min_input_ = 0;
    max_input_ = std::numeric_limits<int>::max();
    min_output_ = 0;
    max_output_ = std::numeric_limits<int>::max();
    // In default, any in-place operation is neither allowed nor enforced.
    inplace_allowed_ = [](int, int) { return false; };
    inplace_enforced_ = [](int, int) { return false; };
    num_inputs_allowed_ = [](int) { return true; };
    num_outputs_allowed_ = [](int) { return true; };
  }

  string file_;
  int line_;
  int min_input_;
  int max_input_;
  std::function<bool(int)> num_inputs_allowed_;
  int min_output_;
  int max_output_;
  std::function<bool(int)> num_outputs_allowed_;

  std::function<int(int)> calculate_output_;
  std::function<bool(int, int)> inplace_allowed_;
  std::function<bool(int, int)> inplace_enforced_;
};

/**
 * @brief A registry to hold all the operator schemas.
 */
class OpSchemaRegistry {
 public:
  static OpSchema& NewSchema(
      const string& key, const string& file, const int line) {
    auto& m = map();
    if (m.count(key)) {
      const auto& schema = m[key];
      std::cerr << "Trying to register schema with name "
                << key << " from file " << file << " line " << line
                << ", but it is already registered from file "
                << schema.file() << " line " << schema.line();
      abort();
    }
    m.emplace(std::make_pair(key, OpSchema(file, line)));
    return m[key];
  }

  static const OpSchema* Schema(const string& key) {
    auto& m = map();
    if (m.count(key)) {
      return &m[key];
    } else {
      return nullptr;
    }
  }

 private:
  // OpSchemaRegistry should not need to be instantiated.
  OpSchemaRegistry() = delete;
  // Returns the underlying string to OpSchema map. We wrap it inside
  // a function to avoid the statia initialization order fiasco.
  static CaffeMap<string, OpSchema>& map();
};

}  // namespace caffe2

#define OPERATOR_SCHEMA(name)                                                 \
  static OpSchema& CAFFE_ANONYMOUS_VARIABLE(name) =                           \
    OpSchemaRegistry::NewSchema(#name, __FILE__, __LINE__)
#define OPERATOR_SCHEMA_STR(name)                                  \
  static OpSchema& CAFFE_ANONYMOUS_VARIABLE(schema_registration) = \
      OpSchemaRegistry::NewSchema(name, __FILE__, __LINE__)

#endif  // CAFFE2_CORE_OPERATOR_SCHEMA_H_
