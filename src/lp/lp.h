#ifndef NCODE_LP_H
#define NCODE_LP_H

#include <stddef.h>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "../common/common.h"

namespace ncode {
namespace lp {

struct RowIndexTag {};
struct ColIndexTag {};

using ConstraintIndex = Index<RowIndexTag>;
using VariableIndex = Index<ColIndexTag>;

enum SolutionType {
  OPTIMAL,   // The solution is optimal.
  FEASIBLE,  // The solution meets all the constraints, but may not be optimal.
  INFEASIBLE_OR_UNBOUNDED,  // The solver failed to find a solution.
  TIMED_OUT,                // The solver timed out.
};

class Solution {
 public:
  // The value of the objective function.
  double ObjectiveValue() const { return objective_value_; }

  // The value of a variable.
  double VariableValue(VariableIndex variable) const {
    return variables_[variable];
  }

  // The type of the solution.
  SolutionType type() const { return solution_type_; }

 private:
  Solution() : solution_type_(INFEASIBLE_OR_UNBOUNDED), objective_value_(0) {}

  SolutionType solution_type_;
  double objective_value_;
  std::vector<double> variables_;

  friend class Problem;
  DISALLOW_COPY_AND_ASSIGN(Solution);
};

// The direction of the problem.
enum Direction { MINIMIZE, MAXIMIZE };

// A single value in the problem matrix.
struct ProblemMatrixElement {
  ProblemMatrixElement(ConstraintIndex constraint, VariableIndex variable,
                       double value)
      : constraint(constraint), variable(variable), value(value) {}

  ConstraintIndex constraint;
  VariableIndex variable;
  double value;
};

class Problem {
 public:
  static_assert(std::numeric_limits<double>::is_iec559, "IEEE 754 required");
  static constexpr double kInifinity = std::numeric_limits<double>::infinity();
  static constexpr double kNegativeInifinity =
      -std::numeric_limits<double>::infinity();

  // .5% away from the best solution is OK.
  static constexpr double kDefaultMIPToleranceGap = 0.005;

  Problem(Direction direction);
  ~Problem();

  // Adds a new variable to the problem. If the variable is specified to be a
  // binary variable it will take either 0 or 1. Trying to set the range of a
  // binary variable later will CHECK-fail.
  VariableIndex AddVariable(bool binary = false);

  // Adds a new constraint to the problem.
  ConstraintIndex AddConstraint();

  // Name a variable.
  void SetVariableName(VariableIndex variable, const std::string& desc);

  // Name a constraint.
  void SetConstraintName(ConstraintIndex variable, const std::string& desc);

  // Sets the range of a variable.
  void SetVariableRange(VariableIndex variable, double min, double max);

  // Sets the range of a constraint.
  void SetConstraintRange(ConstraintIndex constraint, double min, double max);

  // Sets the coefficients of all variables in the problem's matrix.
  void SetMatrix(const std::vector<ProblemMatrixElement>& matrix_elements);

  // Sets the coefficient of a variable in the objective.
  void SetObjectiveCoefficient(VariableIndex variable, double value);

  // Sets the offset of the objective function.
  void SetObjectiveOffset(double value);

  // Dumps the problem to a file in CPLEX LP format.
  void DumpToFile(const std::string& file);

  // Solves the problem and returns the solution. If the time limit is reached
  // before a solution is found will return a solution that is TIMED_OUT.
  std::unique_ptr<Solution> Solve(
      std::chrono::milliseconds time_limit = std::chrono::milliseconds::max());

  // Sets the MIP tolerance.
  void set_mip_tolerance_gap(double gap) { mip_tolerance_gap_ = gap; }

 private:
  // Implementation-specific opaque handle. This is ugly, but it allows us to
  // keep the actual optimizer-specific implementation in the .cc file. This way
  // the clients do not need to have access to the optimizer headers.
  void* handle_;

  // If the problem is a MIP this is how far (in relative terms) a feasible
  // solution should be from optimality.
  double mip_tolerance_gap_;

  // True if any of the variables are binary.
  bool has_binary_variables_;

  DISALLOW_COPY_AND_ASSIGN(Problem);
};

}  // namespace lp
}  // namespace ncode

#endif
