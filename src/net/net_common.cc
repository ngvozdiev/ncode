#include "net_common.h"

#include <algorithm>
#include <arpa/inet.h>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <utility>

#include "../common/strutil.h"
#include "../common/substitute.h"

namespace ncode {
namespace net {

void AddEdgeToGraph(const std::string& src, const std::string& dst, Delay delay,
                    Bandwidth bw, PBNet* graph) {
  using namespace std::chrono;

  CHECK(src != dst) << "Source same as destination: " << src;
  CHECK(!src.empty() && !dst.empty()) << "Source or destination ID missing.";
  PBGraphLink* edge = graph->add_links();
  edge->set_src(src);
  edge->set_dst(dst);
  edge->set_delay_sec(duration<double>(delay).count());
  edge->set_bandwidth_bps(bw.bps());

  uint32_t port_num = graph->links_size();
  edge->set_src_port(port_num);
  edge->set_dst_port(port_num);
}

void AddBiEdgeToGraph(const std::string& src, const std::string& dst,
                      Delay delay, Bandwidth bw, PBNet* graph) {
  AddEdgeToGraph(src, dst, delay, bw, graph);
  AddEdgeToGraph(dst, src, delay, bw, graph);
}

static void AddEdgesToGraphHelper(
    const std::vector<std::pair<std::string, std::string>>& edges, Delay delay,
    Bandwidth bw, bool bidirectional, PBNet* graph) {
  for (const auto& src_and_dst : edges) {
    const std::string& src = src_and_dst.first;
    const std::string& dst = src_and_dst.second;
    if (bidirectional) {
      AddBiEdgeToGraph(src, dst, delay, bw, graph);
    } else {
      AddEdgeToGraph(src, dst, delay, bw, graph);
    }
  }
}

void AddEdgesToGraph(
    const std::vector<std::pair<std::string, std::string>>& edges, Delay delay,
    Bandwidth bw, PBNet* graph) {
  AddEdgesToGraphHelper(edges, delay, bw, false, graph);
}

void AddBiEdgesToGraph(
    const std::vector<std::pair<std::string, std::string>>& edges, Delay delay,
    Bandwidth bw, PBNet* graph) {
  AddEdgesToGraphHelper(edges, delay, bw, true, graph);
}

std::chrono::microseconds TotalDelayOfLinks(const Links& links,
                                            const GraphStorage* link_storage) {
  std::chrono::microseconds total(0);
  for (GraphLinkIndex link_index : links) {
    const GraphLink* link = link_storage->GetLink(link_index);
    total += link->delay();
  }

  return total;
}

GraphLinkIndex GraphStorage::FindUniqueInverseOrDie(const GraphLink* link) {
  const std::string& src = GetNode(link->src())->id();
  const std::string& dst = GetNode(link->dst())->id();

  const auto& dst_to_links = FindOrDie(links_, dst);
  const Links& links = FindOrDie(dst_to_links, src);
  CHECK(links.size() == 1) << "Double edge";
  return links.front();
}

const GraphLink* GraphStorage::GetLink(GraphLinkIndex link_index) const {
  return link_store_.GetItemOrDie(link_index).get();
}

const GraphNode* GraphStorage::GetNode(GraphNodeIndex node_index) const {
  return node_store_.GetItemOrDie(node_index).get();
}

GraphNodeIndex GraphStorage::NodeFromString(const std::string& id) {
  auto it = nodes_.find(id);
  if (it != nodes_.end()) {
    return it->second;
  }

  auto node_ptr = std::unique_ptr<GraphNode>(new GraphNode(id));
  GraphNodeIndex index = node_store_.MoveItem(std::move(node_ptr));
  nodes_[id] = index;
  return index;
}

GraphNodeIndex GraphStorage::NodeFromStringOrDie(const std::string& id) const {
  return FindOrDie(nodes_, id);
}

GraphLinkIndex GraphStorage::LinkFromProtobuf(const PBGraphLink& link_pb) {
  // First try to find the link by the src and dst.
  CHECK(!link_pb.src().empty() && !link_pb.dst().empty())
      << "Link source or destination missing";
  CHECK(link_pb.src() != link_pb.dst()) << "Link source same as destination: "
                                        << link_pb.src();
  auto it_one = links_.find(link_pb.src());
  if (it_one != links_.end()) {
    auto it_two = it_one->second.find(link_pb.dst());
    if (it_two != it_one->second.end()) {
      if (link_pb.src_port() == 0 && link_pb.dst_port() == 0) {
        return it_two->second.front();
      }

      // There are one or many links with the same src and dst addresses.
      const GraphLink* other_with_same_src_port = nullptr;
      const GraphLink* other_with_same_dst_port = nullptr;
      GraphLinkIndex other_with_same_src_port_index;
      for (GraphLinkIndex link_index : it_two->second) {
        const GraphLink* link_ptr = GetLink(link_index);

        if (link_pb.src_port() != 0 &&
            link_ptr->src_port().Raw() == link_pb.src_port()) {
          CHECK(other_with_same_src_port == nullptr);
          other_with_same_src_port = link_ptr;
          other_with_same_src_port_index = link_index;
        }

        if (link_pb.dst_port() != 0 &&
            link_ptr->dst_port().Raw() == link_pb.dst_port()) {
          CHECK(other_with_same_dst_port == nullptr);
          other_with_same_dst_port = link_ptr;
        }
      }

      CHECK(other_with_same_src_port == other_with_same_dst_port);
      if (other_with_same_src_port != nullptr) {
        return other_with_same_src_port_index;
      }
    }
  }

  // Unable to find a link, need to create new one. At this point the protobuf
  // needs to have the ports set.
  CHECK(link_pb.src_port() != 0 && link_pb.dst_port() != 0)
      << "Source or destination port missing for new link from "
      << link_pb.src() << " to " << link_pb.dst();

  auto src_index = NodeFromString(link_pb.src());
  auto dst_index = NodeFromString(link_pb.dst());
  auto link_ptr = std::unique_ptr<GraphLink>(new GraphLink(
      link_pb, src_index, dst_index, GetNode(src_index), GetNode(dst_index)));
  GraphLinkIndex index = link_store_.MoveItem(std::move(link_ptr));
  links_[link_pb.src()][link_pb.dst()].emplace_back(index);
  return index;
}

const GraphLink* GraphStorage::LinkPtrFromProtobuf(const PBGraphLink& link_pb) {
  GraphLinkIndex link_index = LinkFromProtobuf(link_pb);
  return GetLink(link_index);
}

LinkSequence::LinkSequence() : delay_(0) {}

LinkSequence::LinkSequence(const Links& links, const GraphStorage* storage)
    : links_(links), delay_(TotalDelayOfLinks(links, storage)) {
  links_sorted_ = links_;
  std::sort(links_sorted_.begin(), links_sorted_.end());
  if (links_sorted_.size() > 1) {
    for (size_t i = 0; i < links_sorted_.size() - 1; ++i) {
      CHECK(links_sorted_[i] != links_sorted_[i + 1])
          << "Duplicate link in LinkSequence: " << links_sorted_[i];
    }
  }
}

bool LinkSequence::Contains(GraphLinkIndex link) const {
  return std::binary_search(links_sorted_.begin(), links_sorted_.end(), link);
}

void LinkSequence::ToProtobuf(const GraphStorage* storage,
                              net::PBPath* out) const {
  for (GraphLinkIndex link_index : links_) {
    const GraphLink* link = storage->GetLink(link_index);
    *out->add_links() = link->ToProtobuf();
  }
}

std::string LinkSequence::ToString(const GraphStorage* storage) const {
  std::stringstream ss;
  ss << "[";

  for (const auto& edge : links_) {
    const GraphLink* link = storage->GetLink(edge);
    ss << link->ToString();

    if (edge != links_.back()) {
      ss << ", ";
    }
  }

  ss << "]";
  return ss.str();
}

std::string LinkSequence::ToStringNoPorts(const GraphStorage* storage) const {
  std::stringstream ss;
  if (links_.empty()) {
    return "[]";
  }

  ss << "[";
  for (const auto& edge : links_) {
    const GraphLink* link = storage->GetLink(edge);
    ss << link->src_node()->id() << "->";
  }

  const GraphLink* link = storage->GetLink(links_.back());
  ss << link->dst_node()->id();
  ss << "]";
  return ss.str();
}

size_t LinkSequence::InMemBytesEstimate() const {
  return 2 * links_.capacity() * sizeof(Links::value_type) + sizeof(*this);
}

std::string GraphPath::ToString() const {
  return link_sequence_.ToString(storage_);
}

std::string GraphPath::ToStringNoPorts() const {
  using namespace std::chrono;
  double delay_ms = duration<double, milliseconds::period>(delay()).count();
  return Substitute("$0 $1ms", link_sequence_.ToStringNoPorts(storage_),
                    delay_ms);
}

GraphNodeIndex GraphPath::first_hop() const {
  GraphLinkIndex first_link = link_sequence_.links().front();
  return storage_->GetLink(first_link)->src();
}

GraphNodeIndex GraphPath::last_hop() const {
  GraphLinkIndex last_link = link_sequence_.links().back();
  return storage_->GetLink(last_link)->dst();
}

const GraphPath* PathStorage::PathFromString(const std::string& path_string,
                                             const PBNet& graph,
                                             uint64_t cookie) {
  CHECK(path_string.length() > 1) << "Path string malformed: " << path_string;
  CHECK(path_string.front() == '[' && path_string.back() == ']')
      << "Path string malformed: " << path_string;

  std::string inner = path_string.substr(1, path_string.size() - 2);
  if (inner.empty()) {
    // Empty path
    return empty_path_.get();
  }

  std::vector<std::string> edge_strings = Split(inner, ", ");
  Links links;

  for (const auto& edge_string : edge_strings) {
    std::vector<std::string> src_and_dst = Split(edge_string, "->");

    CHECK(src_and_dst.size() == 2) << "Path string malformed: " << path_string;
    std::string src = src_and_dst[0];
    std::string dst = src_and_dst[1];
    CHECK(src.size() > 0 && dst.size() > 0) << "Path string malformed: "
                                            << path_string;

    GraphLinkIndex edge_index;
    bool found = false;
    for (const auto& edge : graph.links()) {
      if (edge.src() == src && edge.dst() == dst) {
        edge_index = LinkFromProtobuf(edge);
        found = true;
      }
    }

    CHECK(found) << "Link missing from graph: " << edge_string;
    links.push_back(edge_index);
  }

  return PathFromLinks({links, this}, cookie);
}

const GraphPath* PathStorage::PathFromLinks(const LinkSequence& link_sequence,
                                            uint64_t cookie) {
  if (link_sequence.empty()) {
    return empty_path_.get();
  }

  const GraphPath* return_path;
  std::map<Links, GraphPath>& path_map = cookie_to_paths_[cookie];
  auto iterator_and_added = path_map.emplace(
      std::piecewise_construct, std::forward_as_tuple(link_sequence.links()),
      std::forward_as_tuple(this));
  if (iterator_and_added.second) {
    auto& it = iterator_and_added.first;
    it->second.Populate(link_sequence, ++tag_generator_);
    return_path = &(it->second);
  } else {
    auto& it = iterator_and_added.first;
    return_path = &(it->second);
  }

  return return_path;
}

const GraphPath* PathStorage::PathFromProtobuf(
    const google::protobuf::RepeatedPtrField<PBGraphLink>& links_pb,
    uint64_t cookie) {
  Links links;
  const std::string* old_dst = nullptr;
  for (const auto& link_pb : links_pb) {
    links.push_back(LinkFromProtobuf(link_pb));

    const std::string& src = link_pb.src();
    const std::string& dst = link_pb.dst();

    CHECK(old_dst == nullptr || *old_dst == src) << "Path not contiguous";
    old_dst = &dst;
  }

  return PathFromLinks({links, this}, cookie);
}

const GraphPath* PathStorage::PathFromProtobuf(
    const std::vector<PBGraphLink>& links, uint64_t cookie) {
  google::protobuf::RepeatedPtrField<PBGraphLink> links_pb;
  for (const auto& link : links) {
    *links_pb.Add() = link;
  }

  return PathFromProtobuf(links_pb, cookie);
}

std::string PathStorage::DumpPaths() const {
  using namespace std::chrono;

  static std::stringstream out;
  for (const auto& cookie_and_path_map : cookie_to_paths_) {
    const std::map<Links, GraphPath>& links_to_path =
        cookie_and_path_map.second;

    for (const auto& links_and_path : links_to_path) {
      const GraphPath& path = links_and_path.second;
      double delay_ms =
          duration<double, milliseconds::period>(path.delay()).count();
      out << Substitute("$0|$1|$2|$3\n", path.ToStringNoPorts(), path.tag(),
                        cookie_and_path_map.first, delay_ms);
    }
  }

  return out.str();
}

const GraphPath* PathStorage::FindPathByTagOrNull(uint32_t tag) const {
  for (const auto& cookie_and_path_map : cookie_to_paths_) {
    const std::map<Links, GraphPath>& links_to_path =
        cookie_and_path_map.second;

    for (const auto& links_and_path : links_to_path) {
      const GraphPath& path = links_and_path.second;
      if (path.tag() == tag) {
        return &path;
      }
    }
  }

  return nullptr;
}

bool IsInPaths(const std::string& needle, const PBNet& graph,
               const std::vector<LinkSequence>& haystack,
               PathStorage* storage) {
  const GraphPath* path = storage->PathFromString(needle, graph, 0);

  for (const LinkSequence& path_in_haystack : haystack) {
    if (path_in_haystack.links() == path->link_sequence().links()) {
      return true;
    }
  }

  return false;
}

bool IsInPaths(const std::string& needle, const PBNet& graph,
               const std::vector<const GraphPath*>& haystack, uint64_t cookie,
               PathStorage* storage) {
  const GraphPath* path = storage->PathFromString(needle, graph, cookie);

  for (const GraphPath* path_in_haystack : haystack) {
    if (path_in_haystack == path) {
      return true;
    }
  }

  return false;
}

const FiveTuple FiveTuple::kDefaultTuple = {};

FiveTuple::FiveTuple(const PBFiveTuple& five_tuple_pb)
    : ip_src_(five_tuple_pb.ip_src()),
      ip_dst_(five_tuple_pb.ip_dst()),
      ip_proto_(five_tuple_pb.ip_proto()),
      src_port_(five_tuple_pb.src_port()),
      dst_port_(five_tuple_pb.dst_port()),
      hash_(GetHash(ip_src_, ip_dst_, ip_proto_, src_port_, dst_port_)) {
  CHECK(five_tuple_pb.ip_proto() <= IPProto::Max().Raw())
      << "Bad IP protocol number";
  CHECK(five_tuple_pb.src_port() <= AccessLayerPort::Max().Raw())
      << "Bad source port";
  CHECK(five_tuple_pb.dst_port() <= AccessLayerPort::Max().Raw())
      << "Bad destination port";
}

PBFiveTuple FiveTuple::ToProtobuf() const {
  PBFiveTuple five_tuple;
  five_tuple.set_ip_src(ip_src_.Raw());
  five_tuple.set_ip_dst(ip_dst_.Raw());
  five_tuple.set_ip_proto(ip_proto_.Raw());
  five_tuple.set_src_port(src_port_.Raw());
  five_tuple.set_dst_port(dst_port_.Raw());
  return five_tuple;
}

std::string FiveTuple::ToString() const {
  std::stringstream ss;
  ss << *this;
  return ss.str();
}

std::ostream& operator<<(std::ostream& output, const FiveTuple& op) {
  output << Substitute("(src: $0, dst: $1, proto: $2, sport: $3, dport: $4)",
                       IPToStringOrDie(op.ip_src()),
                       IPToStringOrDie(op.ip_dst()), op.ip_proto().Raw(),
                       op.src_port().Raw(), op.dst_port().Raw());

  return output;
}

bool operator==(const FiveTuple& a, const FiveTuple& b) {
  return std::tie(a.ip_src_, a.ip_dst_, a.ip_proto_, a.src_port_,
                  a.dst_port_) ==
         std::tie(b.ip_src_, b.ip_dst_, b.ip_proto_, b.src_port_, b.dst_port_);
}

bool operator!=(const FiveTuple& a, const FiveTuple& b) {
  return std::tie(a.ip_src_, a.ip_dst_, a.ip_proto_, a.src_port_,
                  a.dst_port_) !=
         std::tie(b.ip_src_, b.ip_dst_, b.ip_proto_, b.src_port_, b.dst_port_);
}

bool operator<(const FiveTuple& a, const FiveTuple& b) {
  return std::tie(a.ip_src_, a.ip_dst_, a.ip_proto_, a.src_port_, a.dst_port_) <
         std::tie(b.ip_src_, b.ip_dst_, b.ip_proto_, b.src_port_, b.dst_port_);
}

bool operator>(const FiveTuple& a, const FiveTuple& b) {
  return std::tie(a.ip_src_, a.ip_dst_, a.ip_proto_, a.src_port_, a.dst_port_) >
         std::tie(b.ip_src_, b.ip_dst_, b.ip_proto_, b.src_port_, b.dst_port_);
}

std::string IPToStringOrDie(IPAddress ip) {
  char str[INET_ADDRSTRLEN];
  uint32_t address = htonl(ip.Raw());
  const char* return_ptr = inet_ntop(AF_INET, &address, str, INET_ADDRSTRLEN);
  CHECK(return_ptr != nullptr) << "Unable to convert IPv4 to string: "
                               << strerror(errno);
  return std::string(str);
}

IPAddress StringToIPOrDie(const std::string& str) {
  uint32_t address;
  int return_value = inet_pton(AF_INET, str.c_str(), &address);
  CHECK(return_value != 0) << "Invalid IPv4 string: " << str;
  CHECK(return_value != -1) << "Unable to convert string to IPv4: "
                            << strerror(errno);
  CHECK(return_value == 1);
  return IPAddress(ntohl(address));
}

IPAddress MaskAddress(IPAddress ip_address, uint8_t mask_len) {
  CHECK(mask_len <= kMaxIPAddressMaskLen);
  uint8_t slack = kMaxIPAddressMaskLen - mask_len;
  uint64_t mask = ~((1 << slack) - 1);
  return IPAddress(ip_address.Raw() & mask);
}

IPRange::IPRange(IPAddress address, uint8_t mask_len)
    : base_address_(IPAddress::Zero()) {
  Init(address, mask_len);
}

IPRange::IPRange(const std::string& range_str)
    : base_address_(IPAddress::Zero()) {
  std::vector<std::string> pieces = Split(range_str, kDelimiter);
  CHECK(pieces.size() == 2) << "Wrong number of delimited pieces";
  const std::string& address_str = pieces.front();
  const std::string& mask_str = pieces.back();

  uint32_t mask_len;
  CHECK(safe_strtou32(mask_str, &mask_len)) << "Bad mask: " << mask_str;
  Init(StringToIPOrDie(address_str), mask_len);
}

std::string IPRange::ToString() const {
  return StrCat(IPToStringOrDie(base_address_), "/", SimpleItoa(mask_len_));
}

void IPRange::Init(IPAddress address, uint8_t mask_len) {
  mask_len_ = mask_len;
  base_address_ = MaskAddress(address, mask_len);
}

// Returns the index of the cluster a node belongs to.
static size_t IndexOfClusterOrDie(const PBNet& graph, const std::string& node) {
  size_t cluster_index = std::numeric_limits<size_t>::max();
  for (int i = 0; i < graph.clusters_size(); ++i) {
    for (const std::string& cluster_node : graph.clusters(i).nodes()) {
      if (cluster_node == node) {
        cluster_index = i;
      }
    }
  }

  CHECK(cluster_index != std::numeric_limits<size_t>::max())
      << "Node not in any cluster: " << node;
  return cluster_index;
}

std::set<std::string> NodesInSameClusterOrDie(const PBNet& graph,
                                              const std::string& node) {
  size_t cluster_index = IndexOfClusterOrDie(graph, node);

  std::set<std::string> return_set;
  for (const std::string& cluster_node :
       graph.clusters(cluster_index).nodes()) {
    if (cluster_node != node) {
      return_set.emplace(cluster_node);
    }
  }

  return return_set;
}

std::set<std::string> NodesInOtherClustersOrDie(const PBNet& graph,
                                                const std::string& node) {
  int cluster_index = IndexOfClusterOrDie(graph, node);

  std::set<std::string> return_set;
  for (int i = 0; i < graph.clusters_size(); ++i) {
    if (i != cluster_index) {
      const auto& nodes_in_cluster = graph.clusters(i).nodes();
      return_set.insert(nodes_in_cluster.begin(), nodes_in_cluster.end());
    }
  }

  return return_set;
}

bool IsNodeInGraph(const PBNet& graph, const std::string& node) {
  for (const auto& link : graph.links()) {
    if (link.src() == node || link.dst() == node) {
      return true;
    }
  }

  return false;
}

bool IsIntraClusterLink(const PBNet& graph, const PBGraphLink& link) {
  const std::string& src = link.src();
  std::set<std::string> in_same_cluster = NodesInSameClusterOrDie(graph, src);
  return ContainsKey(in_same_cluster, link.dst());
}

GraphLink::GraphLink(const net::PBGraphLink& link_pb, GraphNodeIndex src,
                     GraphNodeIndex dst, const GraphNode* src_node,
                     const GraphNode* dst_node)
    : src_(src),
      dst_(dst),
      src_port_(link_pb.src_port()),
      dst_port_(link_pb.dst_port()),
      bandwidth_(Bandwidth::FromBitsPerSecond(link_pb.bandwidth_bps())),
      src_node_(src_node),
      dst_node_(dst_node) {
  using namespace std::chrono;
  CHECK(link_pb.delay_sec() != 0) << "Link has zero delay";
  CHECK(link_pb.bandwidth_bps() != 0) << "Link has zero bandwidth";
  duration<double> duration(link_pb.delay_sec());
  delay_ = duration_cast<Delay>(duration);
}

std::string GraphLink::ToString() const {
  return Substitute("$0:$1->$2:$3", src_node_->id(), src_port_.Raw(),
                    dst_node_->id(), dst_port_.Raw());
}

PBGraphLink GraphLink::ToProtobuf() const {
  PBGraphLink out_pb;
  out_pb.set_src(src_node_->id());
  out_pb.set_dst(dst_node_->id());
  out_pb.set_src_port(src_port_.Raw());
  out_pb.set_dst_port(dst_port_.Raw());
  out_pb.set_bandwidth_bps(bandwidth_.bps());

  double delay_sec =
      std::chrono::duration_cast<std::chrono::duration<double>>(delay_).count();
  out_pb.set_delay_sec(delay_sec);
  return out_pb;
}

PBGraphLink* FindEdgeOrDie(const std::string& src, const std::string& dst,
                           PBNet* net) {
  for (PBGraphLink& link : *net->mutable_links()) {
    if (link.src() == src && link.dst() == dst) {
      return &link;
    }
  }

  LOG(FATAL) << "No edge from " << src << " to " << dst;
  return nullptr;
}

bool IsPartitioned(const PBNet& graph) {
  std::map<std::string, size_t> node_to_index;
  size_t i = 0;
  for (const auto& link : graph.links()) {
    if (!ContainsKey(node_to_index, link.src())) {
      node_to_index[link.src()] = i++;
    }

    if (!ContainsKey(node_to_index, link.dst())) {
      node_to_index[link.dst()] = i++;
    }
  }

  const size_t inf = std::numeric_limits<size_t>::max();
  size_t nodes = node_to_index.size();
  std::vector<std::vector<size_t>> distances(nodes);
  for (size_t i = 0; i < nodes; ++i) {
    distances[i] = std::vector<size_t>(nodes, inf);
  }

  for (const auto& link : graph.links()) {
    size_t src_index = node_to_index[link.src()];
    size_t dst_index = node_to_index[link.dst()];
    distances[src_index][dst_index] = 1;
  }

  for (size_t k = 0; k < nodes; ++k) {
    distances[k][k] = 0;
  }

  for (size_t k = 0; k < nodes; ++k) {
    for (size_t i = 0; i < nodes; ++i) {
      for (size_t j = 0; j < nodes; ++j) {
        size_t via_k = (distances[i][k] == inf || distances[k][j] == inf)
                           ? inf
                           : distances[i][k] + distances[k][j];

        if (distances[i][j] > via_k) {
          distances[i][j] = via_k;
        }
      }
    }
  }

  // If the distances contain any std::numeric_limits<size_t>::max() then the
  // network is partitioned.
  for (size_t i = 0; i < nodes; ++i) {
    for (size_t j = 0; j < nodes; ++j) {
      if (distances[i][j] == inf) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace net
}  // namespace ncode
