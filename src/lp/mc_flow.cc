#include "mc_flow.h"

#include <map>
#include <memory>
#include <string>

#include "../common/logging.h"
#include "../common/map_util.h"
#include "lp.h"

namespace ncode {
namespace lp {

MCProblem::MCProblem(const net::GraphStorage* graph_storage,
                     double capacity_multiplier)
    : graph_storage_(graph_storage), capacity_multiplier_(capacity_multiplier) {
  for (net::GraphLinkIndex link_index : graph_storage->AllLinks()) {
    const net::GraphLink* link = graph_storage->GetLink(link_index);

    net::GraphNodeIndex out = link->src();
    net::GraphNodeIndex in = link->dst();
    adjacent_to_v_[out].first.emplace_back(link_index);
    adjacent_to_v_[in].second.emplace_back(link_index);
  }
}

MCProblem::MCProblem(const MCProblem& mc_problem, double scale_factor,
                     double increment) {
  graph_storage_ = mc_problem.graph_storage_;
  capacity_multiplier_ = mc_problem.capacity_multiplier_;
  adjacent_to_v_ = mc_problem.adjacent_to_v_;

  for (const Commodity& commodity : mc_problem.commodities_) {
    Commodity scaled_commodity = commodity;
    scaled_commodity.demand *= scale_factor;
    scaled_commodity.demand += increment;
    commodities_.emplace_back(scaled_commodity);
  }
}

MCProblem::VarMap MCProblem::GetLinkToVariableMap(
    Problem* problem, std::vector<ProblemMatrixElement>* problem_matrix) {
  VarMap link_to_variables;

  // There will be a variable per-link per-commodity.
  for (net::GraphLinkIndex link_index : graph_storage_->AllLinks()) {
    const net::GraphLink* link = graph_storage_->GetLink(link_index);

    // One constraint per link to make sure the sum of all commodities over it
    // fit the capacity of the link.
    ConstraintIndex link_constraint = problem->AddConstraint();

    double scaled_limit = link->bandwidth().bps() * capacity_multiplier_;
    problem->SetConstraintRange(link_constraint, 0, scaled_limit);

    for (size_t commodity_index = 0; commodity_index < commodities_.size();
         ++commodity_index) {
      VariableIndex var = problem->AddVariable();
      problem->SetVariableRange(var, 0, Problem::kInifinity);
      link_to_variables[link_index].emplace_back(var);

      problem_matrix->emplace_back(link_constraint, var, 1.0);
    }
  }

  return link_to_variables;
}

static VariableIndex GetVar(const MCProblem::VarMap& var_map,
                            net::GraphLinkIndex edge, size_t commodity_index) {
  const auto& vars = var_map[edge];
  return vars[commodity_index];
}

void MCProblem::AddFlowConservationConstraints(
    const VarMap& link_to_variables, Problem* problem,
    std::vector<ProblemMatrixElement>* problem_matrix) {
  // Per-commodity flow conservation.
  for (size_t c_index = 0; c_index < commodities_.size(); ++c_index) {
    const Commodity& commodity = commodities_[c_index];

    for (const auto& node_and_adj_lists : adjacent_to_v_) {
      net::GraphNodeIndex node = node_and_adj_lists.first;
      const std::vector<net::GraphLinkIndex>& edges_out =
          node_and_adj_lists.second->first;
      const std::vector<net::GraphLinkIndex>& edges_in =
          node_and_adj_lists.second->second;

      // For all nodes except the source and the sink the sum of the flow into
      // the node should be equal to the sum of the flow out. All flow into the
      // source should be 0. All flow out of the sink should be 0.
      ConstraintIndex flow_conservation_constraint = problem->AddConstraint();
      problem->SetConstraintRange(flow_conservation_constraint, 0, 0);

      if (node == commodity.source) {
        for (net::GraphLinkIndex edge_in : edges_in) {
          VariableIndex var = GetVar(link_to_variables, edge_in, c_index);
          problem_matrix->emplace_back(flow_conservation_constraint, var, 1.0);
        }

        // Traffic that leaves the source should sum up to at least the demand
        // of the commodity.
        ConstraintIndex source_load_constraint = problem->AddConstraint();
        problem->SetConstraintRange(source_load_constraint, commodity.demand,
                                    Problem::kInifinity);

        for (net::GraphLinkIndex edge_out : edges_out) {
          VariableIndex var = GetVar(link_to_variables, edge_out, c_index);
          problem_matrix->emplace_back(source_load_constraint, var, 1.0);
        }

      } else if (node == commodity.sink) {
        for (net::GraphLinkIndex edge_out : edges_out) {
          VariableIndex var = GetVar(link_to_variables, edge_out, c_index);
          problem_matrix->emplace_back(flow_conservation_constraint, var, 1.0);
        }
      } else {
        for (net::GraphLinkIndex edge_out : edges_out) {
          VariableIndex var = GetVar(link_to_variables, edge_out, c_index);
          problem_matrix->emplace_back(flow_conservation_constraint, var, -1.0);
        }

        for (net::GraphLinkIndex edge_in : edges_in) {
          VariableIndex var = GetVar(link_to_variables, edge_in, c_index);
          problem_matrix->emplace_back(flow_conservation_constraint, var, 1.0);
        }
      }
    }
  }
}

std::vector<std::vector<FlowAndPath>> MCProblem::RecoverPaths(
    const VarMap& link_to_variables, const lp::Solution& solution) const {
  std::vector<std::vector<FlowAndPath>> out;

  for (size_t c_index = 0; c_index < commodities_.size(); ++c_index) {
    const Commodity& commodity = commodities_[c_index];
    out.emplace_back();

    net::GraphLinkMap<double> link_to_flow;
    for (const auto& link_and_variables : link_to_variables) {
      net::GraphLinkIndex link = link_and_variables.first;
      const std::vector<VariableIndex>& variables = *link_and_variables.second;
      CHECK(variables.size() > c_index);

      double flow = solution.VariableValue(variables[c_index]);
      if (flow > 0) {
        link_to_flow[link] = flow;
      }
    }

    net::Links links;
    double starting_flow = commodity.demand > 0
                               ? commodity.demand
                               : std::numeric_limits<double>::max();
    RecoverPathsRecursive(&link_to_flow, c_index, commodity.source, &links,
                          starting_flow, &out.back());
  }

  return out;
}

void MCProblem::RecoverPathsRecursive(
    net::GraphLinkMap<double>* flow_over_links, size_t c_index,
    net::GraphNodeIndex at_node, net::Links* links_so_far, double flow_on_path,
    std::vector<FlowAndPath>* out) const {
  const Commodity& commodity = commodities_[c_index];
  if (at_node == commodity.sink) {
    CHECK(flow_on_path != std::numeric_limits<double>::max());
    if (commodity.demand > 0) {
      CHECK(flow_on_path <= commodity.demand);
    }

    auto new_path = net::LinkSequence(*links_so_far, graph_storage_);
    out->emplace_back(flow_on_path, new_path);
    return;
  }

  const auto& adj_lists = adjacent_to_v_.GetValueOrDie(at_node);
  const std::vector<net::GraphLinkIndex>& edges_out = adj_lists.first;
  for (net::GraphLinkIndex edge_out : edges_out) {
    if (!flow_over_links->HasValue(edge_out)) {
      continue;
    }

    const net::GraphLink* edge = graph_storage_->GetLink(edge_out);
    double& remaining = flow_over_links->GetValueOrDie(edge_out);
    double to_take = std::min(remaining, flow_on_path);
    if (to_take > 0) {
      links_so_far->emplace_back(edge_out);
      remaining -= to_take;
      RecoverPathsRecursive(flow_over_links, c_index, edge->dst(), links_so_far,
                            to_take, out);
      links_so_far->pop_back();
    }
  }
}

void MCProblem::AddCommodity(const std::string& source, const std::string& sink,
                             double demand) {
  commodities_.push_back({graph_storage_->NodeFromStringOrDie(source),
                          graph_storage_->NodeFromStringOrDie(sink), demand});
}

void MCProblem::AddCommodity(ncode::net::GraphNodeIndex source,
                             ncode::net::GraphNodeIndex sink, double demand) {
  commodities_.push_back({source, sink, demand});
}

bool MCProblem::IsFeasible() {
  Problem problem(MAXIMIZE);
  std::vector<ProblemMatrixElement> problem_matrix;
  VarMap link_to_variables = GetLinkToVariableMap(&problem, &problem_matrix);
  AddFlowConservationConstraints(link_to_variables, &problem, &problem_matrix);

  // Solve the problem.
  problem.SetMatrix(problem_matrix);
  std::unique_ptr<Solution> solution = problem.Solve();
  return solution->type() == ncode::lp::OPTIMAL ||
         solution->type() == ncode::lp::FEASIBLE;
}

static constexpr double kMaxScaleFactor = 10000000.0;
static constexpr double kStopThreshold = 0.0001;

double MCProblem::MaxCommodityScaleFactor() {
  if (!IsFeasible()) {
    return 0;
  }

  bool all_zero = true;
  for (const Commodity& commodity : commodities_) {
    if (commodity.demand != 0) {
      all_zero = false;
      break;
    }
  }

  if (all_zero) {
    return 0;
  }

  // Will do a binary search to look for the problem that is the closes to being
  // infeasible in a given number of steps.
  double min_bound = 1.0;
  double max_bound = kMaxScaleFactor;

  double curr_estimate = kMaxScaleFactor;
  while (true) {
    CHECK(max_bound >= min_bound);
    double delta = max_bound - min_bound;
    if (delta <= kStopThreshold) {
      break;
    }

    double guess = min_bound + (max_bound - min_bound) / 2;
    MCProblem test_problem(*this, guess, 0.0);

    bool is_feasible = test_problem.IsFeasible();
    if (is_feasible) {
      curr_estimate = guess;
      min_bound = guess;
    } else {
      max_bound = guess;
    }
  }

  return curr_estimate;
}

double MCProblem::MaxCommodityIncrement() {
  if (!IsFeasible() || commodities_.empty()) {
    return 0;
  }

  // The initial increment will be the max of the cpacity of all links.
  uint64_t max_capacity = 0;
  for (net::GraphLinkIndex link_index : graph_storage_->AllLinks()) {
    const net::GraphLink* link = graph_storage_->GetLink(link_index);

    if (link->bandwidth().bps() > max_capacity) {
      max_capacity = link->bandwidth().bps();
    }
  }

  double min_bound = 1.0;
  double max_bound = max_capacity;

  double curr_estimate = max_capacity;
  while (true) {
    CHECK(max_bound >= min_bound);
    double delta = max_bound - min_bound;
    if (delta <= kStopThreshold) {
      break;
    }

    double guess = min_bound + (max_bound - min_bound) / 2;
    MCProblem test_problem(*this, 1.0, guess);

    bool is_feasible = test_problem.IsFeasible();
    if (is_feasible) {
      curr_estimate = guess;
      min_bound = guess;
    } else {
      max_bound = guess;
    }
  }

  return curr_estimate;
}

bool MaxFlowMCProblem::GetMaxFlow(
    double* max_flow, std::vector<std::vector<FlowAndPath>>* paths) {
  Problem problem(MAXIMIZE);
  std::vector<ProblemMatrixElement> problem_matrix;
  VarMap link_to_variables = GetLinkToVariableMap(&problem, &problem_matrix);
  AddFlowConservationConstraints(link_to_variables, &problem, &problem_matrix);

  for (size_t commodity_index = 0; commodity_index < commodities_.size();
       ++commodity_index) {
    const Commodity& commodity = commodities_[commodity_index];

    for (const auto& link_and_variables : link_to_variables) {
      net::GraphLinkIndex link_index = link_and_variables.first;
      const net::GraphLink* link = graph_storage_->GetLink(link_index);
      VariableIndex var = (*link_and_variables.second)[commodity_index];

      if (link->src() == commodity.source) {
        // If this link leaves the source we will add it to the objective
        // function -- we want to maximize flow over all commodities.
        problem.SetObjectiveCoefficient(var, 1.0);
      }
    }
  }

  // Solve the problem.
  problem.SetMatrix(problem_matrix);
  std::unique_ptr<Solution> solution = problem.Solve();
  if (solution->type() != ncode::lp::OPTIMAL &&
      solution->type() != ncode::lp::FEASIBLE) {
    return false;
  }

  *max_flow = solution->ObjectiveValue();
  if (paths) {
    *paths = RecoverPaths(link_to_variables, *solution);
  }

  return true;
}

}  // namespace lp
}  // namespace ncode
