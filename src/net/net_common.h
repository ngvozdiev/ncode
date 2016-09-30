#ifndef NCODE_NET_COMMON_H
#define NCODE_NET_COMMON_H

#include <google/protobuf/repeated_field.h>
#include <stddef.h>
#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <chrono>

#include "net.pb.h"
#include "../common/common.h"
#include "../common/logging.h"

namespace ncode {
namespace net {

// An IP address is just an integer -- currently v6 addresses are not supported.
struct IPAddressTag {};
using IPAddress = TypesafeUintWrapper<IPAddressTag, uint32_t, '*'>;

// The access-layer port is a 16bit integer.
struct AccessLayerPortTag {};
using AccessLayerPort = TypesafeUintWrapper<AccessLayerPortTag, uint16_t, '*'>;

// The IP protocol is an 8bit integer.
struct IPProtoTag {};
using IPProto = TypesafeUintWrapper<IPProtoTag, uint8_t, '*'>;

// The number of a network port on a device.
struct DevicePortNumberTag {};
using DevicePortNumber =
    TypesafeUintWrapper<DevicePortNumberTag, uint32_t, '*'>;

// Define some constants.
static constexpr IPProto kProtoTCP = IPProto(6);
static constexpr IPProto kProtoUDP = IPProto(17);
static constexpr IPProto kProtoICMP = IPProto(1);

// Adds a single edge to the graph. Useful for testing. Port numbers will be
// auto-assigned.
void AddEdgeToGraph(const std::string& src, const std::string& dst,
                    std::chrono::microseconds delay, uint64_t bw_bps,
                    PBNet* graph);

// Like AddEdgeToGraph, but also adds an edge for dst->src.
void AddBiEdgeToGraph(const std::string& src, const std::string& dst,
                      std::chrono::microseconds delay, uint64_t bw_bps,
                      PBNet* graph);

// A version of AddEdgeToGraph that takes a list of src, dst strings. All edges
// will have the same delay and bw, port numbers will be auto-assigned.
void AddEdgesToGraph(
    const std::vector<std::pair<std::string, std::string>>& edges,
    std::chrono::microseconds delay, uint64_t bw_bps, PBNet* graph);

// Like AddEdgesToGraph, but for each edge also adds an opposite edge.
void AddBiEdgesToGraph(
    const std::vector<std::pair<std::string, std::string>>& edges,
    std::chrono::microseconds delay, uint64_t bw_bps, PBNet* graph);

// Returns a set with the nodes that are in the same cluster as 'node'. If
// 'node' is not in any cluster will die. If it is alone in a cluster will
// return an empty set.
std::set<std::string> NodesInSameClusterOrDie(const PBNet& graph,
                                              const std::string& node);

// Returns a set with all nodes that are not in the same cluster as 'node'. If
// 'node' is not in any cluster will die.
std::set<std::string> NodesInOtherClustersOrDie(const PBNet& graph,
                                                const std::string& node);

// Returns true if a link is between two nodes of the same cluster.
bool IsIntraClusterLink(const PBNet& graph, const PBGraphLink& link);

// Returns true if there is an edge with an endpoint equal to 'node' in the
// graph.
bool IsNodeInGraph(const PBNet& graph, const std::string& node);

// A wrapper for PBGraphLink. There can only be one GraphLink for each link in
// the system (the pointers can be compared).
class GraphLink {
 public:
  const PBGraphLink& link_pb() const { return link_pb_; }

  const std::string& src() const { return link_pb_.src(); }

  const std::string& dst() const { return link_pb_.dst(); }

  DevicePortNumber src_port() const {
    return DevicePortNumber(link_pb_.src_port());
  }

  DevicePortNumber dst_port() const {
    return DevicePortNumber(link_pb_.dst_port());
  }

  std::chrono::microseconds delay() const;

  uint64_t bandwidth_bps() const;

  // Returns a string in the form A:sport->B:dport
  std::string ToString() const;

 private:
  GraphLink(const net::PBGraphLink& link_pb) : link_pb_(link_pb) {}

  // The original protobuf.
  const net::PBGraphLink link_pb_;

  friend class LinkStorage;
  DISALLOW_COPY_AND_ASSIGN(GraphLink);
};

// Just a bunch of links.
using Links = std::vector<const GraphLink*>;

// Sums up the delay along a series of links.
std::chrono::microseconds TotalDelayOfLinks(const Links& links);

// A sequence of links along with a delay. Similar to GraphPath (below), but
// without a tag.
class LinkSequence {
 public:
  LinkSequence();

  LinkSequence(const Links& links);

  bool Contains(const net::GraphLink* link) const;

  // The delay of all links in this sequence.
  std::chrono::microseconds delay() const { return delay_; }

  // Number of links in the sequence.
  size_t size() const { return links_.size(); }

  // Whether or not there are any links in the sequence.
  bool empty() const { return links_.empty(); }

  // The list of links.
  const Links& links() const { return links_; }

  // Saves this sequence to a protobuf path.
  void ToProtobuf(net::PBPath* out) const;

  // String representation in the form [A:p1->B:p2, B:p3->C:p3]
  std::string ToString() const;

  // Shorter string representation in the form [A->B->C]
  std::string ToStringNoPorts() const;

  // Rough estimate of the number of bytes of memory this LinkSequence uses.
  size_t InMemBytesEstimate() const;

 private:
  // The links in this sequence.
  Links links_;

  // The sum of the delay values of all links.
  std::chrono::microseconds delay_;

  // The links, sorted by pointer. Used by Contains.
  Links links_sorted_;
};

class PathStorage;

// GraphPaths are heavier versions of LinkSequence that are assigned ids and are
// managed by a PathStorage instance.
class GraphPath {
 public:
  // String representation in the form [A:p1->B:p2, B:p3->C:p3]
  std::string ToString() const;

  // String representation in the form A -> B -> C
  std::string ToStringNoPorts() const;

  // The underlying link sequence.
  const LinkSequence& link_sequence() const { return link_sequence_; }

  // Delay of the path.
  std::chrono::microseconds delay() const { return link_sequence_.delay(); }

  // True if path is empty.
  bool empty() const { return link_sequence_.links().empty(); }

  // Number of links.
  uint32_t size() const { return link_sequence_.links().size(); }

  // The storage object that keeps track of all paths.
  PathStorage* storage() const { return storage_; }

  // A tag that identifies the path. Two paths with the same tag will have the
  // same memory location.
  uint32_t tag() const { return tag_; }

  // Id of the first node along the path.
  const std::string& first_hop() const {
    return link_sequence_.links()[0]->src();
  }

  // Id of the last node along the path.
  const std::string& last_hop() const {
    return link_sequence_.links()[link_sequence_.links().size() - 1]->dst();
  }

  // Constructs an initially empty path.
  GraphPath(PathStorage* storage) : tag_(0), storage_(storage) {}

  // Populates this object.
  void Populate(LinkSequence link_sequence, uint32_t tag) {
    link_sequence_ = link_sequence;
    tag_ = tag;
  }

 private:
  // The sequence of links that form this path.
  LinkSequence link_sequence_;

  // A number uniquely identifying the path.
  uint32_t tag_;

  // The parent storage.
  PathStorage* storage_;

  DISALLOW_COPY_AND_ASSIGN(GraphPath);
};

struct PathComparator {
  bool operator()(const GraphPath* p1, const GraphPath* p2) {
    return p1->delay() < p2->delay();
  }
};

// Stores and maintains links.
class LinkStorage {
 public:
  LinkStorage() {}

  // At least the src and the dst need to be populated in the link_pb.
  // If there is no link between src and dst the ports need to also be populated
  // in order to create a new one.
  const GraphLink* LinkFromProtobuf(const PBGraphLink& link_pb);

  // Attempts to find the unique inverse of a link. If the link has no inverse,
  // or has multiple inverses will die.
  const GraphLink* FindUniqueInverseOrDie(const GraphLink* link);

 private:
  // A map from src to dst to a list of links between that (src, dst) pair. The
  // list will only contain more than one element if double edges are used.
  typedef std::vector<std::unique_ptr<GraphLink>> LinksList;
  std::map<std::string, std::map<std::string, LinksList>> links;

  DISALLOW_COPY_AND_ASSIGN(LinkStorage);
};

// Stores paths and assigns tags to them. Will return the same path object for
// the same sequence of links within the same aggregate. The same paths within
// different aggregates will get assigned different tags and map to different
// path objects.
class PathStorage : public LinkStorage {
 public:
  PathStorage() : tag_generator_(0) {
    empty_path_ = std::unique_ptr<GraphPath>(new GraphPath(this));
  }

  // Returns a graph path from a string of the form [A->B, B->C]. Port
  // numbers cannot be specified -- do not use if double edges are possible.
  const GraphPath* PathFromString(const std::string& path_string,
                                  const PBNet& graph,
                                  uint64_t aggregate_cookie);

  // Retuns a graph path from a sequence of links.
  const GraphPath* PathFromLinks(const LinkSequence& links,
                                 uint64_t aggregate_cookie);

  // From a protobuf repetated field.
  const GraphPath* PathFromProtobuf(
      const google::protobuf::RepeatedPtrField<PBGraphLink>& links,
      uint64_t aggregate_cookie);

  // From a vector with protobufs.
  const GraphPath* PathFromProtobuf(const std::vector<PBGraphLink>& links,
                                    uint64_t aggregate_cookie);

  // Returns the empty path. There is only one empty path instance per
  // PathStorage.
  const GraphPath* EmptyPath() { return empty_path_.get(); }

  // Dumps all paths in this path storage to a string.
  std::string DumpPaths() const;

  // Finds a path given its tag.
  const GraphPath* FindPathByTagOrNull(uint32_t tag) const;

 private:
  // Non-empty paths. Grouped by aggregate cookie and then by links in the path.
  std::map<uint64_t, std::map<Links, GraphPath>> cookie_to_paths_;

  // There is only one empty path instance.
  std::unique_ptr<GraphPath> empty_path_;

  // Tags come from here.
  uint32_t tag_generator_;

  template <typename T>
  friend class std::allocator;

  DISALLOW_COPY_AND_ASSIGN(PathStorage);
};

// A convenience function equivalent to calling StringToPath followed by
// std::find to check if haystack contains the path needle.
bool IsInPaths(const std::string& needle, const PBNet& graph,
               const std::vector<LinkSequence>& haystack, PathStorage* storage);

bool IsInPaths(const std::string& needle, const PBNet& graph,
               const std::vector<const GraphPath*>& haystack, uint64_t cookie,
               PathStorage* storage);

// A five-tuple is a combination of ip src/dst, access layer src/dst ports and
// protocol type. It uniquely identifies an IP connection and can be used for
// matching.
class FiveTuple {
 public:
  static const FiveTuple kDefaultTuple;

  static constexpr std::size_t GetHash(IPAddress ip_src, IPAddress ip_dst,
                                       IPProto ip_proto,
                                       AccessLayerPort src_port,
                                       AccessLayerPort dst_port) {
    return 37 * (37 * (37 * (37 * (37 * 17 + ip_proto.Raw()) + ip_src.Raw()) +
                       ip_dst.Raw()) +
                 src_port.Raw()) +
           dst_port.Raw();
  }

  FiveTuple(const PBFiveTuple& five_tuple_pb);

  constexpr FiveTuple()
      : ip_src_(0),
        ip_dst_(0),
        ip_proto_(0),
        src_port_(0),
        dst_port_(0),
        hash_(0) {}

  constexpr FiveTuple(IPAddress ip_src, IPAddress ip_dst, IPProto ip_proto,
                      AccessLayerPort src_port, AccessLayerPort dst_port)
      : ip_src_(ip_src),
        ip_dst_(ip_dst),
        ip_proto_(ip_proto),
        src_port_(src_port),
        dst_port_(dst_port),
        hash_(GetHash(ip_src, ip_dst, ip_proto, src_port, dst_port)) {}

  // The access layer destination port.
  AccessLayerPort dst_port() const { return dst_port_; }

  // The IP destination address.
  IPAddress ip_dst() const { return ip_dst_; }

  // The IP protocol.
  IPProto ip_proto() const { return ip_proto_; }

  // The IP source address.
  IPAddress ip_src() const { return ip_src_; }

  // The access layer source port.
  AccessLayerPort src_port() const { return src_port_; }

  // This tuple's hash value.
  std::size_t hash() const { return hash_; }

  // Returns a FiveTuple that will match the other side of the connection that
  // this tuple matches. The new tuple will have src/dst address and ports
  // swapped.
  FiveTuple Reverse() const {
    return FiveTuple(ip_dst_, ip_src_, ip_proto_, dst_port_, src_port_);
  }

  // Returns a protobuf with the 5-tuple.
  PBFiveTuple ToProtobuf() const;

  // Same as the << operator, but returns a string.
  std::string ToString() const;

  // Pretty-printing comparion and equality.
  friend std::ostream& operator<<(std::ostream& output, const FiveTuple& op);
  friend bool operator==(const FiveTuple& a, const FiveTuple& b);
  friend bool operator!=(const FiveTuple& a, const FiveTuple& b);
  friend bool operator<(const FiveTuple& a, const FiveTuple& b);
  friend bool operator>(const FiveTuple& a, const FiveTuple& b);

 private:
  IPAddress ip_src_;
  IPAddress ip_dst_;
  IPProto ip_proto_;
  AccessLayerPort src_port_;
  AccessLayerPort dst_port_;

  // The hash value of the tuple is cached here.
  std::size_t hash_;
};

// Hashes a FiveTuple.
struct FiveTupleHasher {
  std::size_t operator()(const FiveTuple& k) const { return k.hash(); }
};

// Converts between strings and IPAddresses.
std::string IPToStringOrDie(IPAddress ip);
IPAddress StringToIPOrDie(const std::string& str);

// Applies a mask to a given IPAddress.
static constexpr uint8_t kMaxIPAddressMaskLen = 32;
IPAddress MaskAddress(IPAddress ip_address, uint8_t mask);

// An IPv4 range (combination of address and mask).
class IPRange {
 public:
  static constexpr const char* kDelimiter = "/";

  IPRange(IPAddress address, uint8_t mask_len);

  IPRange(const std::string& range_str);

  // The number of bits in the mask.
  uint8_t mask_len() const { return mask_len_; }

  // The base address.
  IPAddress base_address() const { return base_address_; }

  std::string ToString() const;

 private:
  void Init(IPAddress address, uint8_t mask_len);

  IPAddress base_address_;
  uint8_t mask_len_;
};

}  // namespace net
}  // namespace ncode

#endif