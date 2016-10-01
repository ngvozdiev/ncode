#include "net_common.h"

#include <cstdint>
#include <limits>
#include <vector>
#include <thread>

#include "../common/substitute.h"
#include "gtest/gtest.h"

namespace ncode {
namespace net {
namespace test {

static constexpr char kSrc[] = "A";
static constexpr char kDst[] = "B";
static constexpr net::DevicePortNumber kSrcNetPort = net::DevicePortNumber(10);
static constexpr net::DevicePortNumber kDstNetPort = net::DevicePortNumber(20);
static constexpr std::chrono::milliseconds kDelay(20);
static constexpr uint32_t kBw = 200000;

static constexpr IPAddress kSrcIp = IPAddress(1);
static constexpr IPAddress kDstIp = IPAddress(2);
static constexpr IPProto kProto = IPProto(3);
static constexpr AccessLayerPort kSrcPort = AccessLayerPort(4);
static constexpr AccessLayerPort kDstPort = AccessLayerPort(5);

using namespace std::chrono;

TEST(AddEdgeTest, AddEdgeBadId) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  net::PBNet graph;
  ASSERT_DEATH(AddEdgeToGraph("A", "", milliseconds(10), 100, &graph),
               "missing");
  ASSERT_DEATH(AddEdgeToGraph("", "B", milliseconds(10), 100, &graph),
               "missing");
  ASSERT_DEATH(AddEdgeToGraph("A", "A", milliseconds(10), 100, &graph), "same");
}

TEST(AddEdgeTest, AddEdge) {
  net::PBNet graph;
  AddEdgeToGraph("A", "B", kDelay, kBw, &graph);
  ASSERT_EQ(1, graph.links_size());

  const net::PBGraphLink& link = graph.links(0);
  ASSERT_EQ("A", link.src());
  ASSERT_EQ("B", link.dst());
  ASSERT_EQ(kDelay.count() / 1000.0, link.delay_sec());
  ASSERT_EQ(kBw, link.bandwidth_bps());
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
  ASSERT_EQ(kBw, link_one.bandwidth_bps());

  const net::PBGraphLink& link_two = graph.links(1);
  ASSERT_EQ("B", link_two.src());
  ASSERT_EQ("C", link_two.dst());
  ASSERT_EQ(kDelay.count() / 1000.0, link_two.delay_sec());
  ASSERT_EQ(kBw, link_two.bandwidth_bps());

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

TEST(ClusterTest, GetNodesInSameCluster) {
  net::PBNet graph;
  ASSERT_DEATH(NodesInSameClusterOrDie(graph, "N0"), ".*");

  PBNetCluster* cluster = graph.add_clusters();
  cluster->add_nodes("N1");
  ASSERT_DEATH(NodesInSameClusterOrDie(graph, "N0"), ".*");
  ASSERT_TRUE(NodesInSameClusterOrDie(graph, "N1").empty());

  cluster->add_nodes("N2");
  ASSERT_EQ(NodesInSameClusterOrDie(graph, "N1"),
            std::set<std::string>({"N2"}));

  PBNetCluster* other_cluster = graph.add_clusters();
  other_cluster->add_nodes("N3");
  ASSERT_EQ(NodesInSameClusterOrDie(graph, "N1"),
            std::set<std::string>({"N2"}));
}

TEST(ClusterTest, GetNodesInOtherClusters) {
  net::PBNet graph;
  ASSERT_DEATH(NodesInOtherClustersOrDie(graph, "N0"), ".*");

  PBNetCluster* cluster = graph.add_clusters();
  cluster->add_nodes("N1");
  ASSERT_TRUE(NodesInOtherClustersOrDie(graph, "N1").empty());

  cluster->add_nodes("N2");
  ASSERT_TRUE(NodesInOtherClustersOrDie(graph, "N1").empty());

  PBNetCluster* other_cluster = graph.add_clusters();
  other_cluster->add_nodes("N3");
  ASSERT_EQ(NodesInOtherClustersOrDie(graph, "N1"),
            std::set<std::string>({"N3"}));
}

class LinkStorageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    link_pb_.set_src(kSrc);
    link_pb_.set_dst(kDst);
    link_pb_.set_src_port(kSrcNetPort.Raw());
    link_pb_.set_dst_port(kDstNetPort.Raw());
    link_pb_.set_delay_sec(kDelay.count() / 1000.0);
    link_pb_.set_bandwidth_bps(kBw);
  }

  // Just a random valid link.
  PBGraphLink link_pb_;

  LinkStorage storage_;
};

TEST_F(LinkStorageTest, BadLink) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  PBGraphLink link_pb;
  ASSERT_DEATH(storage_.LinkFromProtobuf(link_pb), "missing");

  link_pb.set_src(kSrc);
  ASSERT_DEATH(storage_.LinkFromProtobuf(link_pb), "missing");

  link_pb.set_dst(kDst);
  ASSERT_DEATH(storage_.LinkFromProtobuf(link_pb), "missing");

  link_pb.set_src_port(kSrcNetPort.Raw());
  ASSERT_DEATH(storage_.LinkFromProtobuf(link_pb), "missing");

  link_pb.set_dst_port(kDstNetPort.Raw());
  ASSERT_NE(nullptr, storage_.LinkFromProtobuf(link_pb));
}

TEST_F(LinkStorageTest, BadLinkDuplicateSrcDst) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  PBGraphLink link_pb = link_pb_;
  link_pb.set_src(kSrc);
  link_pb.set_dst(kSrc);

  ASSERT_DEATH(storage_.LinkFromProtobuf(link_pb), "same");
}

TEST_F(LinkStorageTest, Init) {
  const GraphLink* link = storage_.LinkFromProtobuf(link_pb_);
  ASSERT_EQ(kSrc, link->src());
  ASSERT_EQ(kDst, link->dst());
  ASSERT_EQ(kBw, link->bandwidth_bps());
  ASSERT_EQ(kDelay, link->delay());
  ASSERT_EQ(kDstNetPort, link->dst_port());
  ASSERT_EQ(kSrcNetPort, link->src_port());
}

TEST_F(LinkStorageTest, SameLink) {
  const GraphLink* link = storage_.LinkFromProtobuf(link_pb_);
  const GraphLink* link_two = storage_.LinkFromProtobuf(link_pb_);
  ASSERT_EQ(link, link_two);

  PBGraphLink link_no_port = link_pb_;
  link_no_port.clear_src_port();
  ASSERT_EQ(storage_.LinkFromProtobuf(link_pb_), link);

  link_no_port.clear_dst_port();
  ASSERT_EQ(storage_.LinkFromProtobuf(link_pb_), link);
}

TEST_F(LinkStorageTest, LinkToString) {
  const GraphLink* link = storage_.LinkFromProtobuf(link_pb_);
  ASSERT_EQ(Substitute("$0:$1->$2:$3", kSrc, kSrcNetPort.Raw(), kDst,
                       kDstNetPort.Raw()),
            link->ToString());
}

TEST_F(LinkStorageTest, LinkNoDelay) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  PBGraphLink link_no_delay = link_pb_;
  link_no_delay.clear_delay_sec();

  const GraphLink* link = storage_.LinkFromProtobuf(link_no_delay);
  ASSERT_NE(nullptr, link);
  ASSERT_DEATH(link->delay(), "zero delay");
}

TEST_F(LinkStorageTest, LinkNoBw) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  PBGraphLink link_no_delay = link_pb_;
  link_no_delay.clear_bandwidth_bps();

  const GraphLink* link = storage_.LinkFromProtobuf(link_no_delay);
  ASSERT_NE(nullptr, link);
  ASSERT_DEATH(link->bandwidth_bps(), "zero bandwidth");
}

TEST_F(LinkStorageTest, FindInverse) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  PBGraphLink link_pb = link_pb_;
  const GraphLink* link = storage_.LinkFromProtobuf(link_pb);
  ASSERT_DEATH(storage_.FindUniqueInverseOrDie(link), ".*");

  PBGraphLink inverse_link_pb = link_pb_;
  inverse_link_pb.set_dst(link_pb_.src());
  inverse_link_pb.set_src(link_pb_.dst());

  const GraphLink* inverse_link = storage_.LinkFromProtobuf(inverse_link_pb);
  ASSERT_EQ(inverse_link, storage_.FindUniqueInverseOrDie(link));

  PBGraphLink another_inverse_link_pb = link_pb_;
  another_inverse_link_pb.set_dst(link_pb_.src());
  another_inverse_link_pb.set_src(link_pb_.dst());
  another_inverse_link_pb.set_dst_port(link_pb.dst_port() + 1);
  another_inverse_link_pb.set_src_port(link_pb.src_port() + 1);

  const GraphLink* another_inverse_link =
      storage_.LinkFromProtobuf(another_inverse_link_pb);
  CHECK_NE(another_inverse_link, inverse_link);
  ASSERT_DEATH(storage_.FindUniqueInverseOrDie(link), ".*");
}

TEST(LinkSequence, Empty) {
  LinkSequence link_sequence;
  ASSERT_EQ(0, link_sequence.size());
  ASSERT_LT(0, link_sequence.InMemBytesEstimate());
  ASSERT_TRUE(link_sequence.empty());
  ASSERT_EQ("[]", link_sequence.ToString());
}

TEST_F(LinkStorageTest, LinkSequenceSingleLink) {
  const GraphLink* link = storage_.LinkFromProtobuf(link_pb_);
  Links links = {link};

  LinkSequence link_sequence(links);
  ASSERT_EQ(1, link_sequence.size());
  ASSERT_LT(8, link_sequence.InMemBytesEstimate());
  ASSERT_FALSE(link_sequence.empty());
  ASSERT_EQ(Substitute("[$0]", link->ToString()), link_sequence.ToString());
  ASSERT_EQ("[A->B]", link_sequence.ToStringNoPorts());
}

TEST_F(LinkStorageTest, LinkSequenceBadDuplicateLink) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  const GraphLink* link = storage_.LinkFromProtobuf(link_pb_);
  Links links = {link, link};
  ASSERT_DEATH(LinkSequence sequence(links), "Duplicate link");
}

TEST_F(LinkStorageTest, LinkSequenceToProtobuf) {
  const GraphLink* link = storage_.LinkFromProtobuf(link_pb_);

  PBPath path;
  Links links = {link};

  LinkSequence link_sequence(links);
  link_sequence.ToProtobuf(&path);

  ASSERT_EQ(1, path.links_size());
  ASSERT_EQ(link_pb_.SerializeAsString(), path.links(0).SerializeAsString());
}

class PathStorageTest : public ::testing::Test {
 protected:
  void SetUp() {
    AddEdgeToGraph("A", "B", kDelay, kBw, &graph_);
    AddEdgeToGraph("B", "A", kDelay, kBw, &graph_);
    AddEdgeToGraph("B", "C", kDelay, kBw, &graph_);
  }

  PBNet graph_;
  PathStorage storage_;
};

TEST_F(PathStorageTest, Empty) {
  ASSERT_EQ("", storage_.DumpPaths());
  ASSERT_NE(nullptr, storage_.EmptyPath());
  ASSERT_EQ(0, storage_.EmptyPath()->size());
}

TEST_F(PathStorageTest, EmptyPathFromString) {
  ASSERT_EQ(storage_.EmptyPath(), storage_.PathFromString("[]", graph_, 0));
  ASSERT_EQ(storage_.EmptyPath(), storage_.PathFromString("[]", graph_, 1));
}

TEST_F(PathStorageTest, BadFromString) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  ASSERT_DEATH(storage_.PathFromString("", graph_, 0), "malformed");
  ASSERT_DEATH(storage_.PathFromString("[", graph_, 0), "malformed");
  ASSERT_DEATH(storage_.PathFromString("]", graph_, 0), "malformed");
  ASSERT_DEATH(storage_.PathFromString("[A->B, B->]", graph_, 0), "malformed");
  ASSERT_DEATH(storage_.PathFromString("A->B->C", graph_, 0), "malformed");

  // Missing element
  ASSERT_DEATH(storage_.PathFromString("[AB->D]", graph_, 0), "missing");
  ASSERT_DEATH(storage_.PathFromString("[A->B, B->C, C->D]", graph_, 0),
               "missing");
}

TEST_F(PathStorageTest, FromString) {
  const GraphPath* path_one =
      storage_.PathFromString("[A->B, B->C]", graph_, 0);
  const GraphPath* path_two =
      storage_.PathFromString("[A->B, B->C]", graph_, 1);

  ASSERT_NE(nullptr, path_one);
  ASSERT_NE(nullptr, path_two);
  ASSERT_NE(path_one, path_two);  // Same path, but different cookies.

  const GraphPath* path_three =
      storage_.PathFromString("[A->B, B->C]", graph_, 0);
  ASSERT_EQ(path_one, path_three);
  ASSERT_EQ(path_one->tag(), path_three->tag());
}

TEST_F(PathStorageTest, FindByTag) {
  const GraphPath* path_one =
      storage_.PathFromString("[A->B, B->C]", graph_, 0);
  const GraphPath* path_two =
      storage_.PathFromString("[A->B, B->C]", graph_, 1);

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
  ASSERT_EQ(32, ip_range.mask_len());
  ASSERT_EQ(StringToIPOrDie("1.2.3.4"), ip_range.base_address());
}

TEST(IPRange, Slash16) {
  IPRange ip_range("1.2.3.4/16");
  ASSERT_EQ(16, ip_range.mask_len());
  ASSERT_EQ(StringToIPOrDie("1.2.0.0"), ip_range.base_address());
}

}  // namespace test
}  // namespace net
}  // namespace ncode
