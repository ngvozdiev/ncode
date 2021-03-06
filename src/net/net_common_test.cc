#include "net_common.h"

#include <cstdint>
#include <limits>
#include <vector>
#include <thread>

#include "../common/substitute.h"
#include "gtest/gtest.h"
#include "net_gen.h"

namespace ncode {
namespace net {
namespace test {

static constexpr char kSrc[] = "A";
static constexpr char kDst[] = "B";
static constexpr net::DevicePortNumber kSrcNetPort = net::DevicePortNumber(10);
static constexpr net::DevicePortNumber kDstNetPort = net::DevicePortNumber(20);
static constexpr net::Bandwidth kBw = net::Bandwidth::FromBitsPerSecond(20000);
static constexpr std::chrono::milliseconds kDelay(20);
static constexpr IPAddress kSrcIp = IPAddress(1);
static constexpr IPAddress kDstIp = IPAddress(2);
static constexpr IPProto kProto = IPProto(3);
static constexpr AccessLayerPort kSrcPort = AccessLayerPort(4);
static constexpr AccessLayerPort kDstPort = AccessLayerPort(5);

using namespace std::chrono;

TEST(AddEdgeTest, AddEdgeBadId) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  net::PBNet graph;
  ASSERT_DEATH(AddEdgeToGraph("A", "", milliseconds(10), kBw, &graph),
               "missing");
  ASSERT_DEATH(AddEdgeToGraph("", "B", milliseconds(10), kBw, &graph),
               "missing");
  ASSERT_DEATH(AddEdgeToGraph("A", "A", milliseconds(10), kBw, &graph), "same");
}

TEST(AddEdgeTest, AddEdge) {
  net::PBNet graph;
  AddEdgeToGraph("A", "B", kDelay, kBw, &graph);
  ASSERT_EQ(1, graph.links_size());

  const net::PBGraphLink& link = graph.links(0);
  ASSERT_EQ("A", link.src());
  ASSERT_EQ("B", link.dst());
  ASSERT_EQ(kDelay.count() / 1000.0, link.delay_sec());
  ASSERT_EQ(kBw.bps(), link.bandwidth_bps());
}

TEST(AddEdgeTest, AddEdgeDouble) {
  net::PBNet graph;
  AddEdgeToGraph("A", "B", kDelay, kBw, &graph);
  AddEdgeToGraph("A", "B", kDelay, kBw, &graph);

  ASSERT_EQ(2, graph.links_size());
  const net::PBGraphLink& link_one = graph.links(0);
  const net::PBGraphLink& link_two = graph.links(1);

  ASSERT_EQ("A", link_one.src());
  ASSERT_EQ("A", link_two.src());
  ASSERT_EQ("B", link_one.dst());
  ASSERT_EQ("B", link_two.dst());

  ASSERT_NE(link_one.src_port(), link_two.src_port());
  ASSERT_NE(link_one.dst_port(), link_two.dst_port());
}

TEST(AddEdgeTest, AddEdgesBulk) {
  net::PBNet graph;
  std::vector<std::pair<std::string, std::string>> edges = {{"A", "B"},
                                                            {"B", "C"}};

  AddEdgesToGraph(edges, kDelay, kBw, &graph);
  ASSERT_EQ(2, graph.links_size());

  const net::PBGraphLink& link_one = graph.links(0);
  ASSERT_EQ("A", link_one.src());
  ASSERT_EQ("B", link_one.dst());
  ASSERT_EQ(kDelay.count() / 1000.0, link_one.delay_sec());
  ASSERT_EQ(kBw.bps(), link_one.bandwidth_bps());

  const net::PBGraphLink& link_two = graph.links(1);
  ASSERT_EQ("B", link_two.src());
  ASSERT_EQ("C", link_two.dst());
  ASSERT_EQ(kDelay.count() / 1000.0, link_two.delay_sec());
  ASSERT_EQ(kBw.bps(), link_two.bandwidth_bps());

  ASSERT_NE(link_one.src_port(), link_two.src_port());
  ASSERT_NE(link_one.dst_port(), link_two.dst_port());
}

TEST(AddEdgeTest, AddBiEdgesBulk) {
  net::PBNet graph;
  std::vector<std::pair<std::string, std::string>> edges = {{"A", "B"},
                                                            {"B", "C"}};

  AddBiEdgesToGraph(edges, kDelay, kBw, &graph);
  ASSERT_EQ(4, graph.links_size());
}

TEST(RegionTest, GetNodesInSameRegion) {
  net::PBNet graph;
  ASSERT_DEATH(NodesInSameRegionOrDie(graph, "N0"), ".*");

  PBNetRegion* cluster = graph.add_regions();
  cluster->add_nodes("N1");
  ASSERT_DEATH(NodesInSameRegionOrDie(graph, "N0"), ".*");
  ASSERT_TRUE(NodesInSameRegionOrDie(graph, "N1").empty());

  cluster->add_nodes("N2");
  ASSERT_EQ(NodesInSameRegionOrDie(graph, "N1"), std::set<std::string>({"N2"}));

  PBNetRegion* other_cluster = graph.add_regions();
  other_cluster->add_nodes("N3");
  ASSERT_EQ(NodesInSameRegionOrDie(graph, "N1"), std::set<std::string>({"N2"}));
}

TEST(RegionTest, GetNodesInOtherRegions) {
  net::PBNet graph;
  ASSERT_DEATH(NodesInOtherRegionsOrDie(graph, "N0"), ".*");

  PBNetRegion* cluster = graph.add_regions();
  cluster->add_nodes("N1");
  ASSERT_TRUE(NodesInOtherRegionsOrDie(graph, "N1").empty());

  cluster->add_nodes("N2");
  ASSERT_TRUE(NodesInOtherRegionsOrDie(graph, "N1").empty());

  PBNetRegion* other_cluster = graph.add_regions();
  other_cluster->add_nodes("N3");
  ASSERT_EQ(NodesInOtherRegionsOrDie(graph, "N1"),
            std::set<std::string>({"N3"}));
}

class GraphStorageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    link_pb_.set_src(kSrc);
    link_pb_.set_dst(kDst);
    link_pb_.set_src_port(kSrcNetPort.Raw());
    link_pb_.set_dst_port(kDstNetPort.Raw());
    link_pb_.set_delay_sec(kDelay.count() / 1000.0);
    link_pb_.set_bandwidth_bps(kBw.bps());
  }

  // Just a random valid link.
  PBGraphLink link_pb_;
};

TEST_F(GraphStorageTest, BadLink) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  PBGraphLink link_pb;
  PBNet net;
  *net.add_links() = link_pb;
  ASSERT_DEATH(GraphStorage s(net), "missing");

  link_pb.set_src(kSrc);
  net.Clear();
  *net.add_links() = link_pb;
  ASSERT_DEATH(GraphStorage s(net), "missing");

  link_pb.set_dst(kDst);
  net.Clear();
  *net.add_links() = link_pb;
  ASSERT_DEATH(GraphStorage s(net), "missing");

  link_pb.set_src_port(kSrcNetPort.Raw());
  net.Clear();
  *net.add_links() = link_pb;
  ASSERT_DEATH(GraphStorage s(net), "missing");
}

TEST_F(GraphStorageTest, BadLinkDuplicateSrcDst) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  PBGraphLink link_pb = link_pb_;
  link_pb.set_src(kSrc);
  link_pb.set_dst(kSrc);

  PBNet net;
  *net.add_links() = link_pb;
  ASSERT_DEATH(GraphStorage s(net), "same");
}

TEST_F(GraphStorageTest, Init) {
  PBNet net;
  *net.add_links() = link_pb_;
  GraphStorage storage(net);

  const GraphLink* link = storage.LinkPtrFromProtobufOrDie(link_pb_);
  ASSERT_EQ(kSrc, link->src_node()->id());
  ASSERT_EQ(kDst, link->dst_node()->id());
  ASSERT_EQ(kBw, link->bandwidth());
  ASSERT_EQ(kDelay, link->delay());
  ASSERT_EQ(kDstNetPort, link->dst_port());
  ASSERT_EQ(kSrcNetPort, link->src_port());

  ASSERT_EQ(0ul, storage.Stats().multiple_links);
  ASSERT_EQ(1ul, storage.Stats().unidirectional_links);
}

TEST_F(GraphStorageTest, Stats) {
  PBNet net = GenerateBraess(kBw);
  GraphStorage storage(net);

  ASSERT_EQ(0ul, storage.Stats().multiple_links);
  ASSERT_EQ(1ul, storage.Stats().unidirectional_links);
}

TEST_F(GraphStorageTest, SameLink) {
  PBNet net;
  *net.add_links() = link_pb_;
  GraphStorage storage(net);

  const GraphLink* link = storage.LinkPtrFromProtobufOrDie(link_pb_);
  const GraphLink* link_two = storage.LinkPtrFromProtobufOrDie(link_pb_);
  ASSERT_EQ(link, link_two);

  PBGraphLink link_no_port = link_pb_;
  link_no_port.clear_src_port();

  ASSERT_DEATH(storage.LinkPtrFromProtobufOrDie(link_no_port), ".*");

  link_no_port.clear_dst_port();
  ASSERT_EQ(storage.LinkPtrFromProtobufOrDie(link_no_port), link);
}

TEST_F(GraphStorageTest, LinkToString) {
  PBNet net;
  *net.add_links() = link_pb_;
  GraphStorage storage(net);

  const GraphLink* link = storage.LinkPtrFromProtobufOrDie(link_pb_);
  ASSERT_EQ(Substitute("$0:$1->$2:$3", kSrc, kSrcNetPort.Raw(), kDst,
                       kDstNetPort.Raw()),
            link->ToString());
}

TEST_F(GraphStorageTest, LinkNoDelay) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  PBNet net;
  PBGraphLink link_no_delay = link_pb_;
  link_no_delay.clear_delay_sec();
  *net.add_links() = link_no_delay;
  ASSERT_DEATH(GraphStorage storage(net), "zero delay");
}

TEST_F(GraphStorageTest, LinkNoBw) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  PBNet net;
  PBGraphLink link_no_bw = link_pb_;
  link_no_bw.clear_bandwidth_bps();
  *net.add_links() = link_no_bw;
  ASSERT_DEATH(GraphStorage storage(net), "zero bandwidth");
}

TEST_F(GraphStorageTest, FindInverse) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  PBNet net;
  *net.add_links() = link_pb_;
  GraphStorage storage_one(net);

  PBGraphLink link_pb = link_pb_;
  const GraphLink* link = storage_one.LinkPtrFromProtobufOrDie(link_pb);
  ASSERT_DEATH(storage_one.FindUniqueInverseOrDie(link), ".*");

  PBGraphLink inverse_link_pb = link_pb_;
  inverse_link_pb.set_dst(link_pb_.src());
  inverse_link_pb.set_src(link_pb_.dst());

  *net.add_links() = inverse_link_pb;
  GraphStorage storage_two(net);

  GraphLinkIndex inverse_link =
      storage_two.LinkFromProtobufOrDie(inverse_link_pb);
  ASSERT_EQ(inverse_link, storage_two.FindUniqueInverseOrDie(link));

  PBGraphLink another_inverse_link_pb = link_pb_;
  another_inverse_link_pb.set_dst(link_pb_.src());
  another_inverse_link_pb.set_src(link_pb_.dst());
  another_inverse_link_pb.set_dst_port(link_pb.dst_port() + 1);
  another_inverse_link_pb.set_src_port(link_pb.src_port() + 1);

  *net.add_links() = another_inverse_link_pb;
  GraphStorage storage_three(net);

  GraphLinkIndex another_inverse_link =
      storage_three.LinkFromProtobufOrDie(another_inverse_link_pb);
  CHECK_NE(another_inverse_link, inverse_link);
  ASSERT_DEATH(storage_three.FindUniqueInverseOrDie(link), ".*");
}

TEST_F(GraphStorageTest, Empty) {
  GraphStorage storage((PBNet()));

  LinkSequence link_sequence;
  ASSERT_EQ(0ul, link_sequence.size());
  ASSERT_LT(0ul, link_sequence.InMemBytesEstimate());
  ASSERT_TRUE(link_sequence.empty());
  ASSERT_EQ("[]", link_sequence.ToString(&storage));
}

TEST_F(GraphStorageTest, LinkSequenceSingleLink) {
  PBNet net;
  *net.add_links() = link_pb_;
  GraphStorage storage(net);

  GraphLinkIndex link_index = storage.LinkFromProtobufOrDie(link_pb_);
  const GraphLink* link = storage.GetLink(link_index);
  Links links = {link_index};

  LinkSequence link_sequence(links, TotalDelayOfLinks(links, &storage));
  ASSERT_EQ(1ul, link_sequence.size());
  ASSERT_LT(8ul, link_sequence.InMemBytesEstimate());
  ASSERT_FALSE(link_sequence.empty());
  ASSERT_EQ(Substitute("[$0]", link->ToString()),
            link_sequence.ToString(&storage));
  ASSERT_EQ("[A->B]", link_sequence.ToStringNoPorts(&storage));
}

TEST_F(GraphStorageTest, LinkSequenceBadDuplicateLink) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  PBNet net;
  *net.add_links() = link_pb_;
  GraphStorage storage(net);

  GraphLinkIndex link = storage.LinkFromProtobufOrDie(link_pb_);
  Links links = {link, link};
  ASSERT_DEATH(LinkSequence sequence(links, TotalDelayOfLinks(links, &storage)),
               "Duplicate link");
}

TEST_F(GraphStorageTest, LinkSequenceToProtobuf) {
  PBNet net;
  *net.add_links() = link_pb_;
  GraphStorage storage(net);

  GraphLinkIndex link_index = storage.LinkFromProtobufOrDie(link_pb_);

  PBPath path;
  Links links = {link_index};

  LinkSequence link_sequence(links, TotalDelayOfLinks(links, &storage));
  link_sequence.ToProtobuf(&storage, &path);

  ASSERT_EQ(1, path.links_size());
  ASSERT_EQ(link_pb_.SerializeAsString(), path.links(0).SerializeAsString());
}

TEST_F(GraphStorageTest, BadNodeInRegion) {
  PBNet net = GenerateBraess(kBw);
  PBNetRegion* region = net.add_regions();
  region->add_nodes("F");

  ASSERT_DEATH(GraphStorage storage(net), ".*");
}

TEST_F(GraphStorageTest, NodesInSameRegion) {
  PBNet net = GenerateBraess(kBw);
  GraphStorage storage(net);
  GraphNodeIndex node_a = storage.NodeFromStringOrDie("A");
  GraphNodeIndex node_b = storage.NodeFromStringOrDie("B");

  ASSERT_DEATH(storage.NodesInSameRegionOrDie(node_a), ".*");
  PBNetRegion* region = net.add_regions();
  region->add_nodes("A");
  GraphStorage storage_two(net);
  ASSERT_TRUE(storage_two.NodesInSameRegionOrDie(node_a).Empty());

  region->add_nodes("B");
  GraphStorage storage_three(net);
  auto in_region = storage_three.NodesInSameRegionOrDie(node_a);
  ASSERT_EQ(1ul, in_region.Count());
  ASSERT_TRUE(in_region.Contains(node_b));

  PBNetRegion* other_region = net.add_regions();
  other_region->add_nodes("C");

  GraphStorage storage_four(net);
  in_region = storage_four.NodesInSameRegionOrDie(node_a);
  ASSERT_EQ(1ul, in_region.Count());
  ASSERT_TRUE(in_region.Contains(node_b));
}

static PBNet SetUpGraph() {
  PBNet graph;
  AddEdgeToGraph("A", "B", kDelay, kBw, &graph);
  AddEdgeToGraph("B", "A", kDelay, kBw, &graph);
  AddEdgeToGraph("B", "C", kDelay, kBw, &graph);
  return graph;
}

class PathStorageTest : public ::testing::Test {
 protected:
  PathStorageTest() : storage_(SetUpGraph()) {}
  GraphStorage storage_;
};

TEST_F(PathStorageTest, Empty) {
  ASSERT_EQ("", storage_.DumpPaths());
  ASSERT_NE(nullptr, storage_.EmptyPath());
  ASSERT_EQ(0ul, storage_.EmptyPath()->size());
}

TEST_F(PathStorageTest, EmptyPathFromString) {
  ASSERT_EQ(storage_.EmptyPath(), storage_.PathFromStringOrDie("[]", 0));
  ASSERT_EQ(storage_.EmptyPath(), storage_.PathFromStringOrDie("[]", 1));
}

TEST_F(PathStorageTest, BadFromString) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  ASSERT_DEATH(storage_.PathFromStringOrDie("", 0), "malformed");
  ASSERT_DEATH(storage_.PathFromStringOrDie("[", 0), "malformed");
  ASSERT_DEATH(storage_.PathFromStringOrDie("]", 0), "malformed");
  ASSERT_DEATH(storage_.PathFromStringOrDie("[A->B, B->]", 0), "malformed");
  ASSERT_DEATH(storage_.PathFromStringOrDie("A->B->C", 0), "malformed");

  // Missing element
  ASSERT_DEATH(storage_.PathFromStringOrDie("[AB->D]", 0), ".*");
  ASSERT_DEATH(storage_.PathFromStringOrDie("[A->B, B->C, C->D]", 0), ".*");
}

TEST_F(PathStorageTest, FromString) {
  const GraphPath* path_one = storage_.PathFromStringOrDie("[A->B, B->C]", 0);
  const GraphPath* path_two = storage_.PathFromStringOrDie("[A->B, B->C]", 1);

  ASSERT_NE(nullptr, path_one);
  ASSERT_NE(nullptr, path_two);
  ASSERT_NE(path_one, path_two);  // Same path, but different cookies.

  const GraphPath* path_three = storage_.PathFromStringOrDie("[A->B, B->C]", 0);
  ASSERT_EQ(path_one, path_three);
  ASSERT_EQ(path_one->tag(), path_three->tag());
}

TEST_F(PathStorageTest, FindByTag) {
  const GraphPath* path_one = storage_.PathFromStringOrDie("[A->B, B->C]", 0);
  const GraphPath* path_two = storage_.PathFromStringOrDie("[A->B, B->C]", 1);

  ASSERT_EQ(path_one, storage_.FindPathByTagOrNull(path_one->tag()));
  ASSERT_EQ(path_two, storage_.FindPathByTagOrNull(path_two->tag()));
}

class FiveTupleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    five_tuple_pb_.set_ip_src(kSrcIp.Raw());
    five_tuple_pb_.set_ip_dst(kDstIp.Raw());
    five_tuple_pb_.set_ip_proto(kProto.Raw());
    five_tuple_pb_.set_src_port(kSrcPort.Raw());
    five_tuple_pb_.set_dst_port(kDstPort.Raw());
  }

  static void CheckFiveTuple(const FiveTuple& five_tuple) {
    ASSERT_EQ(kSrcIp, five_tuple.ip_src());
    ASSERT_EQ(kDstIp, five_tuple.ip_dst());
    ASSERT_EQ(kProto, five_tuple.ip_proto());
    ASSERT_EQ(kSrcPort, five_tuple.src_port());
    ASSERT_EQ(kDstPort, five_tuple.dst_port());
  }

  PBFiveTuple five_tuple_pb_;
};

TEST_F(FiveTupleTest, Init) {
  FiveTuple five_tuple(kSrcIp, kDstIp, kProto, kSrcPort, kDstPort);
  CheckFiveTuple(five_tuple);
}

TEST_F(FiveTupleTest, InitFromProtobuf) {
  FiveTuple five_tuple(five_tuple_pb_);
  CheckFiveTuple(five_tuple);
}

TEST_F(FiveTupleTest, ToProtobuf) {
  FiveTuple five_tuple(five_tuple_pb_);
  PBFiveTuple five_tuple_pb = five_tuple.ToProtobuf();
  ASSERT_EQ(five_tuple_pb_.SerializeAsString(),
            five_tuple_pb.SerializeAsString());
}

TEST_F(FiveTupleTest, InitFromProtobufBadProto) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  five_tuple_pb_.set_ip_proto(std::numeric_limits<uint8_t>::max() + 1);
  ASSERT_DEATH(FiveTuple five_tuple(five_tuple_pb_), "IP protocol");
}

TEST_F(FiveTupleTest, InitFromProtobufBadSrcPort) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  five_tuple_pb_.set_src_port(AccessLayerPort::Max().Raw() + 1);
  ASSERT_DEATH(FiveTuple five_tuple(five_tuple_pb_), "source port");
}

TEST_F(FiveTupleTest, InitFromProtobufBadDstPort) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  five_tuple_pb_.set_dst_port(AccessLayerPort::Max().Raw() + 1);
  ASSERT_DEATH(FiveTuple five_tuple(five_tuple_pb_), "destination port");
}

TEST(IPRange, BadRanges) {
  ASSERT_DEATH(IPRange r1("1.2.3.4/sdf/32"), ".*");
  ASSERT_DEATH(IPRange r2("1.2.3.4/32fd"), ".*");
  ASSERT_DEATH(IPRange r3("1.2.3.4/56"), ".*");
  ASSERT_DEATH(IPRange r3("1.asdf2.3.4/56"), ".*");
}

TEST(IPRange, Slash32) {
  IPRange ip_range("1.2.3.4/32");
  ASSERT_EQ(32ul, ip_range.mask_len());
  ASSERT_EQ(StringToIPOrDie("1.2.3.4"), ip_range.base_address());
}

TEST(IPRange, Slash16) {
  IPRange ip_range("1.2.3.4/16");
  ASSERT_EQ(16ul, ip_range.mask_len());
  ASSERT_EQ(StringToIPOrDie("1.2.0.0"), ip_range.base_address());
}

TEST(DisjointSets, Empty) {
  std::map<std::string, std::vector<std::string>> key_to_values = {};
  std::vector<std::set<std::string>> out = GetDisjointSets(key_to_values);
  ASSERT_TRUE(out.empty());
}

TEST(DisjointSets, EmptyValues) {
  std::map<std::string, std::vector<uint64_t>> key_to_values = {{"A", {}}};
  ASSERT_DEATH(GetDisjointSets(key_to_values), ".*");
}

TEST(DisjointSets, SingleValue) {
  std::map<std::string, std::vector<uint64_t>> key_to_values = {{"A", {1ul}}};
  std::vector<std::set<std::string>> out = GetDisjointSets(key_to_values);
  std::vector<std::set<std::string>> model = {{"A"}};
  ASSERT_EQ(model, out);
}

TEST(DisjointSets, SingleKey) {
  std::map<std::string, std::vector<uint64_t>> key_to_values = {
      {"A", {1ul, 2ul}}};
  std::vector<std::set<std::string>> out = GetDisjointSets(key_to_values);
  std::vector<std::set<std::string>> model = {{"A"}};
  ASSERT_EQ(model, out);
}

TEST(DisjointSets, MultiKey) {
  std::map<std::string, std::vector<uint64_t>> key_to_values = {
      {"A", {1ul, 2ul}}, {"B", {1ul}}};
  std::vector<std::set<std::string>> out = GetDisjointSets(key_to_values);
  std::vector<std::set<std::string>> model = {{"A", "B"}};
  ASSERT_EQ(model, out);
}

TEST(DisjointSets, MultiKeyDisjoint) {
  std::map<std::string, std::vector<uint64_t>> key_to_values = {
      {"A", {1ul, 2ul}}, {"B", {1ul}}, {"C", {3ul}}};
  std::vector<std::set<std::string>> out = GetDisjointSets(key_to_values);
  std::vector<std::set<std::string>> model = {{"A", "B"}, {"C"}};
  ASSERT_EQ(model, out);
}

TEST(DisjointSets, MultiKeyDisjointTransitive) {
  std::map<std::string, std::vector<uint64_t>> key_to_values = {
      {"A", {1ul, 2ul}}, {"B", {1ul, 3ul}}, {"C", {3ul, 4ul}}, {"D", {5ul}}};
  std::vector<std::set<std::string>> out = GetDisjointSets(key_to_values);
  std::vector<std::set<std::string>> model = {{"A", "B", "C"}, {"D"}};
  ASSERT_EQ(model, out);
}

TEST(DisjointSets, MultiKeyDisjointTransitiveTwo) {
  std::map<std::string, std::vector<uint64_t>> key_to_values = {
      {"A", {1ul, 2ul}},
      {"B", {1ul, 3ul}},
      {"C", {3ul, 4ul}},
      {"D", {5ul, 6ul, 7ul, 8ul}},
      {"E", {5ul, 7ul, 8ul}}};
  std::vector<std::set<std::string>> out = GetDisjointSets(key_to_values);
  std::vector<std::set<std::string>> model = {{"A", "B", "C"}, {"D", "E"}};
  ASSERT_EQ(model, out);
}

TEST(Partitioned, Empty) {
  PBNet net;
  ASSERT_FALSE(IsPartitioned(net));
}

TEST(Partitioned, SingleUniLink) {
  PBNet net;
  net::PBGraphLink* link_pb = net.add_links();
  link_pb->set_src("A");
  link_pb->set_src("B");
  ASSERT_TRUE(IsPartitioned(net));
}

TEST(Partitioned, SingleLink) {
  PBNet net;
  AddBiEdgeToGraph("A", "B", microseconds(10), kBw, &net);
  ASSERT_FALSE(IsPartitioned(net));
}

TEST(Partitioned, Partition) {
  PBNet net;
  AddBiEdgeToGraph("A", "B", microseconds(10), kBw, &net);
  AddBiEdgeToGraph("B", "Z", microseconds(10), kBw, &net);
  AddBiEdgeToGraph("C", "D", microseconds(10), kBw, &net);
  ASSERT_TRUE(IsPartitioned(net));
}

TEST(Partitioned, NoPartition) {
  PBNet net;
  AddBiEdgeToGraph("A", "B", microseconds(10), kBw, &net);
  AddBiEdgeToGraph("B", "Z", microseconds(10), kBw, &net);
  AddBiEdgeToGraph("C", "D", microseconds(10), kBw, &net);
  AddBiEdgeToGraph("C", "Z", microseconds(10), kBw, &net);
  ASSERT_FALSE(IsPartitioned(net));
}

}  // namespace test
}  // namespace net
}  // namespace ncode
