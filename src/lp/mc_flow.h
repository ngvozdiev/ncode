#ifndef NCODE_MC_FLOW_H
#define NCODE_MC_FLOW_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "../net/net_common.h"
#include "lp.h"

namespace ncode {
namespace lp {

// A single commodity in a multi-commodity problem.
struct Commodity {
  net::GraphNodeIndex source;
  net::GraphNodeIndex sink;
  double demand;
};

// Path and flow on a path.
using FlowAndPath = std::pair<double, net::LinkSequence>;

// A multi-commodity flow problem. Edge capacities will be taken from the
// bandwidth values of the links in the graph this object is constructed with
// times a multiplier.
class MCProblem {
 public:
  using VarMap = net::GraphLinkMap<std::vector<VariableIndex>>;

  MCProblem(const net::GraphStorage* graph_storage,
            double capacity_multiplier = 1.0);

  // Adds a single commodity to the network, with a given source and sink. If
  // the demand of the commodity is not specified it is assumed to be 0. The
  // unit of the demand should be in the same units as the link bandwidth * the
  // capacity multiplier used during construction.
  void AddCommodity(const std::string& source, const std::string& sink,
                    double demand = 0);
  void AddCommodity(ncode::net::GraphNodeIndex source,
                    ncode::net::GraphNodeIndex sink, double demand = 0);

  // Returns true if the MC problem is feasible -- if the commodities/demands
  // can fit in the network.
  bool IsFeasible();

  // If all commodities' demands are multiplied by the returned number the
  // problem will be close to being infeasible. Returns 0 if the problem is
  // currently infeasible or all commodities have 0 demands.
  double MaxCommodityScaleFactor();

  // If the returned demand is added to all commodities the problem will be very
  // close to being infeasible.
  double MaxCommodityIncrement();

  // Returns the commodities.
  const std::vector<Commodity>& commodities() const { return commodities_; }

 protected:
  // Returns a map from a graph link to a list of one variable per commodity.
  VarMap GetLinkToVariableMap(
      Problem* problem, std::vector<ProblemMatrixElement>* problem_matrix);

  // Adds flow conservation constraints to the problem.
  void AddFlowConservationConstraints(
      const VarMap& link_to_variables, Problem* problem,
      std::vector<ProblemMatrixElement>* problem_matrix);

  // Recovers the paths from an MC-flow problem. Returns for each commodity the
  // paths and fractions of commodity over each path.
  std::vector<std::vector<FlowAndPath>> RecoverPaths(
      const VarMap& link_to_variables, const lp::Solution& solution) const;

  const net::GraphStorage* graph_storage_;
  double capacity_multiplier_;
  std::vector<Commodity> commodities_;

  // For each node will keep a list of the edges going out of the node and the
  // edges coming into the node.
  net::GraphNodeMap<std::pair<std::vector<net::GraphLinkIndex>,
                              std::vector<net::GraphLinkIndex>>> adjacent_to_v_;

 private:
  // Returns the same problem, but with all commodities' demands multiplied by
  // the given scale factor and increased by 'increment'.
  MCProblem(const MCProblem& other, double scale_factor, double increment);

  // Helper function for RecoverPaths.
  void RecoverPathsRecursive(net::GraphLinkMap<double>* flow_over_links,
                             size_t commodity_index,
                             net::GraphNodeIndex at_node,
                             net::Links* links_so_far, double overall_flow,
                             std::vector<FlowAndPath>* out) const;

  DISALLOW_COPY_AND_ASSIGN(MCProblem);
};

// Solves the multi commodity max flow problem.
class MaxFlowMCProblem : public MCProblem {
 public:
  MaxFlowMCProblem(const net::GraphStorage* graph_storage,
                   double capacity_multiplier = 1.0)
      : MCProblem(graph_storage, capacity_multiplier) {}

  // Populates the maximum flow (in the same units as edge bandwidth *
  // cpacity_multiplier_) for all commodities. If 'paths' is supplied will also
  // populate it with the actual paths for each commodity that will result in
  // the max flow value. If there are commodities that cannot satisfy their
  // demands false is returned and neither 'max_flow' nor 'paths' are modified.
  bool GetMaxFlow(double* max_flow,
                  std::vector<std::vector<FlowAndPath>>* paths = nullptr);
};

}  // namespace lp
}  // namespace ncode
#endif
