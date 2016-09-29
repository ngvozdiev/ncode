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

void AddEdgeToGraph(const std::string& src, const std::string& dst,
                    std::chrono::microseconds delay, uint64_t bw_bps,
                    PBNet* graph) {
  using namespace std::chrono;

  CHECK(src != dst) << "Source same as destination: " << src;
  CHECK(!src.empty() && !dst.empty()) << "Source or destination ID missing.";
  PBGraphLink* edge = graph->add_links();
  edge->set_src(src);
  edge->set_dst(dst);
  edge->set_delay_sec(duration<double>(delay).count());
  edge->set_bandwidth_bps(bw_bps);

  uint32_t port_num = graph->links_size();
  edge->set_src_port(port_num);
  edge->set_dst_port(port_num);
}

void AddBiEdgeToGraph(const std::string& src, const std::string& dst,
                      std::chrono::microseconds delay, uint64_t bw_bps,
                      PBNet* graph) {
  AddEdgeToGraph(src, dst, delay, bw_bps, graph);
  AddEdgeToGraph(dst, src, delay, bw_bps, graph);
}

static void AddEdgesToGraphHelper(
    const std::vector<std::pair<std::string, std::string>>& edges,
    std::chrono::microseconds delay, uint64_t bw_bps, bool bidirectional,
    PBNet* graph) {
  for (const auto& src_and_dst : edges) {
    const std::string& src = src_and_dst.first;
    const std::string& dst = src_and_dst.second;
    if (bidirectional) {
      AddBiEdgeToGraph(src, dst, delay, bw_bps, graph);
    } else {
      AddEdgeToGraph(src, dst, delay, bw_bps, graph);
    }
  }
}

void AddEdgesToGraph(
    const std::vector<std::pair<std::string, std::string>>& edges,
    std::chrono::microseconds delay, uint64_t bw_bps, PBNet* graph) {
  AddEdgesToGraphHelper(edges, delay, bw_bps, false, graph);
}

void AddBiEdgesToGraph(
    const std::vector<std::pair<std::string, std::string>>& edges,
    std::chrono::microseconds delay, uint64_t bw_bps, PBNet* graph) {
  AddEdgesToGraphHelper(edges, delay, bw_bps, true, graph);
}

std::chrono::microseconds TotalDelayOfLinks(
    const std::vector<const GraphLink*>& links) {
  std::chrono::microseconds total(0);
  for (const GraphLink* link : links) {
    total += link->delay();
  }

  return total;
}

const GraphLink* LinkStorage::FindUniqueInverseOrDie(const GraphLink* link) {
  const std::string& src = link->src();
  const std::string& dst = link->dst();

  const auto& dst_to_links = FindOrDie(links, dst);
  const LinksList& links = FindOrDie(dst_to_links, src);
  CHECK(links.size() == 1) << "Double edge";
  return links.front().get();
}

const GraphLink* LinkStorage::LinkFromProtobuf(const PBGraphLink& link_pb) {
  // First try to find the link by the src and dst.
  CHECK(!link_pb.src().empty() && !link_pb.dst().empty())
      << "Link source or destination missing";
  CHECK(link_pb.src() != link_pb.dst()) << "Link source same as destination: "
                                        << link_pb.src();
  auto it_one = links.find(link_pb.src());
  if (it_one != links.end()) {
    auto it_two = it_one->second.find(link_pb.dst());
    if (it_two != it_one->second.end()) {
      // There are one or many links with the same src and dst addresses.
      for (const auto& link_ptr : it_two->second) {
        if (link_pb.src_port() != 0 &&
            link_ptr->src_port().Raw() != link_pb.src_port()) {
          continue;
        }

        if (link_pb.dst_port() != 0 &&
            link_ptr->dst_port().Raw() != link_pb.dst_port()) {
          continue;
        }

        return link_ptr.get();
      }
    }
  }

  // Unable to find a link, need to create new one. At this point the protobuf
  // needs to have the ports set.
  CHECK(link_pb.src_port() != 0 && link_pb.dst_port() != 0)
      << "Source or destination port missing for new link from "
      << link_pb.src() << " to " << link_pb.dst();

  GraphLink* return_ptr = new GraphLink(link_pb);
  links[link_pb.src()][link_pb.dst()].emplace_back(
      std::unique_ptr<GraphLink>(return_ptr));
  return return_ptr;
}

LinkSequence::LinkSequence() : delay_(0) {}

LinkSequence::LinkSequence(const Links& links)
    : links_(links), delay_(TotalDelayOfLinks(links)) {
  links_sorted_ = links_;
  std::sort(links_sorted_.begin(), links_sorted_.end());
  if (links_sorted_.size() > 1) {
    for (size_t i = 0; i < links_sorted_.size() - 1; ++i) {
      CHECK(links_sorted_[i] != links_sorted_[i + 1])
          << "Duplicate link in LinkSequence: " << links_sorted_[i]->ToString();
    }
  }
}

bool LinkSequence::Contains(const net::GraphLink* link) const {
  return std::binary_search(links_sorted_.begin(), links_sorted_.end(), link);
}

void LinkSequence::ToProtobuf(net::PBPath* out) const {
  for (const net::GraphLink* link : links_) {
    *out->add_links() = link->link_pb();
  }
}

std::string LinkSequence::ToString() const {
  std::stringstream ss;
  ss << "[";

  for (const auto& edge : links_) {
    ss << edge->ToString();

    if (edge != links_.back()) {
      ss << ", ";
    }
  }

  ss << "]";
  return ss.str();
}

std::string LinkSequence::ToStringNoPorts() const {
  std::stringstream ss;
  if (links_.empty()) {
    return "[]";
  }

  ss << "[";
  for (const auto& edge : links_) {
    ss << edge->src() << "->";
  }

  ss << links_.back()->dst();
  ss << "]";
  return ss.str();
}

size_t LinkSequence::InMemBytesEstimate() const {
  return 2 * links_.capacity() * sizeof(Links::value_type) + sizeof(*this);
}

std::string GraphPath::ToString() const { return link_sequence_.ToString(); }

std::string GraphPath::ToStringNoPorts() const {
  using namespace std::chrono;
  double delay_ms = duration<double, milliseconds::period>(delay()).count();
  return Substitute("$0 $1ms", link_sequence_.ToStringNoPorts(), delay_ms);
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

  std::vector<std::string> edge_strings;
  Links links;

  SplitStringDelimiter(inner, ", ", &edge_strings);
  for (const auto& edge_string : edge_strings) {
    std::vector<std::string> src_and_dst;
    SplitStringDelimiter(edge_string, "->", &src_and_dst);

    CHECK(src_and_dst.size() == 2) << "Path string malformed: " << path_string;
    std::string src = src_and_dst[0];
    std::string dst = src_and_dst[1];
    CHECK(src.size() > 0 && dst.size() > 0) << "Path string malformed: "
                                            << path_string;

    const GraphLink* edge_ptr = nullptr;
    for (const auto& edge : graph.links()) {
      if (edge.src() == src && edge.dst() == dst) {
        edge_ptr = LinkFromProtobuf(edge);
      }
    }

    CHECK(edge_ptr != nullptr) << "Link missing from graph: " << edge_string;
    links.push_back(edge_ptr);
  }

  return PathFromLinks(links, cookie);
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

  return PathFromLinks(links, cookie);
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

std::chrono::microseconds GraphLink::delay() const {
  CHECK(link_pb_.delay_sec() != 0) << "Link has zero delay";
  std::chrono::duration<double> duration(link_pb_.delay_sec());
  return std::chrono::duration_cast<std::chrono::microseconds>(duration);
}

uint64_t GraphLink::bandwidth_bps() const {
  CHECK(link_pb_.bandwidth_bps() != 0) << "Link has zero bandwidth";
  return link_pb_.bandwidth_bps();
}

std::string GraphLink::ToString() const {
  return Substitute("$0:$1->$2:$3", link_pb_.src(), link_pb_.src_port(),
                    link_pb_.dst(), link_pb_.dst_port());
}

}  // namespace net
}  // namespace ncode
