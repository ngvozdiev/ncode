#include "net_gen.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>

#include "net.pb.h"
#include "../common/strutil.h"

namespace ncode {
namespace net {

using namespace std::chrono;

static void AddLink(const PBGraphLink& link_template, const std::string& src,
                    microseconds delay, const std::string& dst,
                    microseconds delay_add, double delay_multiply, size_t* i,
                    PBNet* out) {
  microseconds total_delay =
      duration_cast<microseconds>(delay * delay_multiply) + delay_add;
  total_delay = std::max(microseconds(1), total_delay);
  double total_delay_sec = duration<double>(total_delay).count();

  PBGraphLink link_pb = link_template;
  link_pb.set_src(src);
  link_pb.set_dst(dst);
  link_pb.set_delay_sec(total_delay_sec);

  size_t ports = *i;
  link_pb.set_src_port(ports);
  link_pb.set_dst_port(ports);
  ++(*i);
  *(out->add_links()) = link_pb;
}

static void AddBiLink(const PBGraphLink& link, const std::string& src,
                      double delay_sec, const std::string& dst,
                      microseconds delay_add, double delay_multiply, size_t* i,
                      PBNet* out) {
  auto delay = duration_cast<microseconds>(duration<double>(delay_sec));
  AddLink(link, src, delay, dst, delay_add, delay_multiply, i, out);
  AddLink(link, dst, delay, src, delay_add, delay_multiply, i, out);
}

static void AddBiLink(const PBGraphLink& link, const std::string& src,
                      microseconds delay, const std::string& dst,
                      microseconds delay_add, double delay_multiply, size_t* i,
                      PBNet* out) {
  AddLink(link, src, delay, dst, delay_add, delay_multiply, i, out);
  AddLink(link, dst, delay, src, delay_add, delay_multiply, i, out);
}

static void AddCluster(const std::string& name,
                       const std::vector<std::string>& ids, PBNet* out) {
  PBNetCluster* cluster = out->add_clusters();
  cluster->set_id(name);
  for (const std::string& id : ids) {
    cluster->add_nodes(id);
  }
}

PBNet GenerateHE(const PBGraphLink& link_template, microseconds delay_add,
                 double delay_multiply) {
  PBNet out;

  AddCluster("Asia", {"HongKong", "Singapore", "Tokyo"}, &out);
  AddCluster("North America",
             {"LosAngeles", "SanJose", "Fremont", "PaloAlto", "Portland",
              "Seattle", "Denver", "Vancouver", "Phoenix", "LasVegas",
              "Minneapolis", "KansasCity", "Dallas", "Chicago", "Ashburn",
              "Atlanta", "Miami", "NewYork", "Toronto"},
             &out);
  AddCluster("Europe", {"London", "Paris", "Amsterdam", "Frankfurt am Main",
                        "Stockholm", "Berlin", "Zurich", "Prague", "Warsaw"},
             &out);

  size_t i = 1;
  AddBiLink(link_template, "HongKong", 0.09899999999999999, "LosAngeles",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "HongKong", 0.02197946590441512, "Singapore",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "HongKong", 0.024491937340699977, "Tokyo", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Tokyo", 0.045182269059776056, "Singapore",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "SanJose", 0.07082018626712423, "Tokyo", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Fremont", 0.00020944320149470966, "SanJose",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "PaloAlto", 0.0002098538430130341, "SanJose",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "PaloAlto", 0.00015330384758825228, "Fremont",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Portland", 0.007750976557599637, "SanJose",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Seattle", 0.00970358928467878, "SanJose", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Seattle", 0.001980189853586593, "Portland",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Seattle", 0.0016290555332783203, "Vancouver",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Denver", 0.013942438885259888, "Seattle", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "LosAngeles", 0.004379930992404047, "PaloAlto",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Phoenix", 0.004878751961226585, "LosAngeles",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "LasVegas", 0.003127585787117664, "LosAngeles",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Denver", 0.008271265381124625, "LasVegas",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Denver", 0.012679134906052427, "SanJose", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Denver", 0.009549009601746527, "Minneapolis",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Denver", 0.00761386951297863, "KansasCity",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Dallas", 0.012085544227624674, "Phoenix", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Dallas", 0.019896482083487595, "Fremont", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Dallas", 0.006205167979753415, "KansasCity",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Dallas", 0.010971931036320862, "Chicago", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Dallas", 0.01588028050776086, "Ashburn", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Dallas", 0.009845557514101322, "Atlanta", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Dallas", 0.015180864669691761, "Miami", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Miami", 0.00827928825214428, "Atlanta", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Ashburn", 0.007244818676274683, "Atlanta",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "KansasCity", 0.005613545439041082, "Chicago",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "NewYork", 0.009734695650495928, "Chicago",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Toronto", 0.0059679334825478835, "Chicago",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Minneapolis", 0.004854188292269742, "Chicago",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Ashburn", 0.0029763178724357656, "NewYork",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Toronto", 0.004719392909665924, "NewYork",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "PaloAlto", 0.03499418206738756, "NewYork",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "SanJose", 0.03485362282421041, "NewYork", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Ashburn", 0.03268078197594825, "SanJose", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "London", 0.04732289625262841, "NewYork", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Ashburn", 0.050272198201872294, "London", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Amsterdam", 0.049801303976444715, "NewYork",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Ashburn", 0.052553436093461164, "Paris", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "London", 0.0029205763165338866, "Paris", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Zurich", 0.004154523412587286, "Paris", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Frankfurt am Main", 0.004065115464977358, "Paris",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Amsterdam", 0.003657458411802766, "Paris",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Amsterdam", 0.0030323776775934013, "London",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Frankfurt am Main", 0.005416362162592938, "London",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Amsterdam", 0.009561504025009172, "Stockholm",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Amsterdam", 0.0049050665652040255, "Berlin",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Amsterdam", 0.0030944299935970203,
            "Frankfurt am Main", delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Amsterdam", 0.005227380278661229, "Zurich",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Prague", 0.0034757563350856072, "Frankfurt am Main",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Zurich", 0.0025991945628872828, "Frankfurt am Main",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(link_template, "Warsaw", 0.004395690813733776, "Prague", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(link_template, "Berlin", 0.004390355474006233, "Warsaw", delay_add,
            delay_multiply, &i, &out);

  return out;
}

net::PBNet GenerateFullGraph(uint32_t size, uint64_t rate_bps,
                             microseconds delay) {
  net::PBNet return_graph;
  double delay_sec = duration<double>(delay).count();
  for (uint32_t i = 0; i < size; ++i) {
    for (uint32_t j = 0; j < size; ++j) {
      if (i != j) {
        net::PBGraphLink* edge = return_graph.add_links();

        edge->set_delay_sec(delay_sec);
        edge->set_src("N" + std::to_string(i));
        edge->set_dst("N" + std::to_string(j));
        edge->set_src_port(i * size + j);
        edge->set_dst_port(i * size + j);
        edge->set_bandwidth_bps(rate_bps);
      }
    }
  }

  return return_graph;
}

net::PBNet GenerateLadder(size_t levels, uint64_t rate_bps, microseconds delay,
                          const std::vector<microseconds>& central_delays) {
  net::PBNet return_graph;
  CHECK(levels > 0) << "Tried to generate ladder with no edges,";

  if (!central_delays.empty()) {
    CHECK(central_delays.size() == levels);
  }

  net::PBGraphLink link_template;
  link_template.set_bandwidth_bps(rate_bps);

  size_t port_gen = 1;
  for (size_t level = 0; level < levels; ++level) {
    std::string node_left = StrCat("N", std::to_string(4 * level));
    std::string node_right = StrCat("N", std::to_string(4 * level + 1));

    microseconds central_link_delay = delay;
    if (!central_delays.empty()) {
      central_link_delay = central_delays[level];
    }

    AddBiLink(link_template, node_left, central_link_delay, node_right,
              microseconds::zero(), 1.0, &port_gen, &return_graph);

    if (level != 0) {
      std::string mid_node_left =
          StrCat("N", std::to_string(4 * (level - 1) + 2));

      std::string mid_node_right =
          StrCat("N", std::to_string(4 * (level - 1) + 3));

      std::string prev_node_left = StrCat("N", std::to_string(4 * (level - 1)));
      std::string prev_node_right =
          StrCat("N", std::to_string(4 * (level - 1) + 1));

      AddBiLink(link_template, node_left, delay, mid_node_left,
                microseconds::zero(), 1.0, &port_gen, &return_graph);
      AddBiLink(link_template, mid_node_left, delay, prev_node_left,
                microseconds::zero(), 1.0, &port_gen, &return_graph);

      AddBiLink(link_template, node_right, delay, mid_node_right,
                microseconds::zero(), 1.0, &port_gen, &return_graph);
      AddBiLink(link_template, mid_node_right, delay, prev_node_right,
                microseconds::zero(), 1.0, &port_gen, &return_graph);
    }
  }

  return return_graph;
}

}  // namespace net
}  // namespace ncode
