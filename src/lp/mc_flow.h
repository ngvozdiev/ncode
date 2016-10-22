#ifndef NCODE_MC_FLOW_H
#define NCODE_MC_FLOW_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "../net/net_common.h"
#include "lp.h"

namespace ncode {
namespace lp {

// A multi-commodity flow problem. Edge capacities will be taken from the
// bandwidth values of the links in the graph this object is constructed with
// times a multiplier.
class MCProblem {
 public:
  using VarMap = std::map<const net::GraphLink*, std::vector<VariableIndex>>;

  MCProblem(const net::PBNet& graph, net::LinkStorage* link_storage,
            double capacity_multiplier = 1.0);

  // Adds a single commodity to the network, with a given source and sink. If
  // the demand of the commodity is not specified it is assumed to be 0. The
  // unit of the demand should be in the same units as the link bandwidth * the
  // capacity multiplier used during construction.
  void AddCommodity(const std::string& source, const std::string& sink,
                    double demand = 0);

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

 protected:
  struct Commodity {
    std::string source;
    std::string sink;
    double demand;
  };

  // Returns a map from a graph link to a list of one variable per commodity.
  VarMap GetLinkToVariableMap(
      Problem* problem, std::vector<ProblemMatrixElement>* problem_matrix);

  // Adds flow conservation constraints to the problem.
  void AddFlowConservationConstraints(
      const VarMap& link_to_variables, Problem* problem,
      std::vector<ProblemMatrixElement>* problem_matrix);

  net::LinkStorage* link_storage_;
  double capacity_multiplier_;
  std::vector<const net::GraphLink*> graph_;
  std::vector<Commodity> commodities_;

  // For each node will keep a list of the edges going out of the node and the
  // edges coming into the node.
  std::map<std::string, std::pair<std::vector<const net::GraphLink*>,
                                  std::vector<const net::GraphLink*>>>
      adjacent_to_v_;

 private:
  // Returns the same problem, but with all commodities' demands multiplied by
  // the given scale factor and increased by 'increment'.
  MCProblem(const MCProblem& other, double scale_factor, double increment);

  bool IsInGraph(const std::string& source);

  DISALLOW_COPY_AND_ASSIGN(MCProblem);
};

// Solves the multi commodity max flow problem.
class MaxFlowMCProblem : public MCProblem {
 public:
  MaxFlowMCProblem(const net::PBNet& graph, net::LinkStorage* link_storage,
                   double capacity_multiplier = 1.0)
      : MCProblem(graph, link_storage, capacity_multiplier) {}

  // Populates the maximum flow (in the same units as edge bandwidth *
  // cpacity_multiplier_) for all commodities. If there are commodities that
  // cannot satisfy their demands false is returned.
  bool GetMaxFlow(double* max_flow);
};

}  // namespace lp
}  // namespace ncode
#endif
