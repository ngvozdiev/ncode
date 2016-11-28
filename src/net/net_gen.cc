#include "net_gen.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>

#include "net.pb.h"
#include "../common/strutil.h"
#include "../common/file.h"

namespace ncode {
namespace net {

using namespace std::chrono;

static void AddLink(Bandwidth bw, const std::string& src, Delay delay,
                    const std::string& dst, Delay delay_add,
                    double delay_multiply, Bandwidth bw_add, double bw_multiply,
                    size_t* i, PBNet* out) {
  microseconds total_delay =
      duration_cast<microseconds>(delay * delay_multiply) + delay_add;
  total_delay = std::max(microseconds(1), total_delay);
  double total_delay_sec = duration<double>(total_delay).count();
  uint64_t total_bw = bw.bps() * bw_multiply + bw_add.bps();

  PBGraphLink link_pb;
  link_pb.set_bandwidth_bps(total_bw);
  link_pb.set_src(src);
  link_pb.set_dst(dst);
  link_pb.set_delay_sec(total_delay_sec);

  size_t ports = *i;
  link_pb.set_src_port(ports);
  link_pb.set_dst_port(ports);
  ++(*i);
  *(out->add_links()) = link_pb;
}

static void AddLink(Bandwidth bw, const std::string& src, double delay_sec,
                    const std::string& dst, Delay delay_add,
                    double delay_multiply, size_t* i, PBNet* out) {
  auto delay = duration_cast<Delay>(duration<double>(delay_sec));
  AddLink(bw, src, delay, dst, delay_add, delay_multiply,
          Bandwidth::FromBitsPerSecond(0), 1.0, i, out);
}

static void AddBiLink(Bandwidth bw, const std::string& src, double delay_sec,
                      const std::string& dst, Delay delay_add,
                      double delay_multiply, size_t* i, PBNet* out) {
  auto delay = duration_cast<Delay>(duration<double>(delay_sec));
  AddLink(bw, src, delay, dst, delay_add, delay_multiply,
          Bandwidth::FromBitsPerSecond(0), 1.0, i, out);
  AddLink(bw, dst, delay, src, delay_add, delay_multiply,
          Bandwidth::FromBitsPerSecond(0), 1.0, i, out);
}

static void AddBiLink(uint64_t bw_bps, const std::string& src, double delay_sec,
                      const std::string& dst, Delay delay_add,
                      double delay_multiply, Bandwidth bw_add,
                      double bw_multiply, size_t* i, PBNet* out) {
  auto delay = duration_cast<microseconds>(duration<double>(delay_sec));
  AddLink(Bandwidth::FromBitsPerSecond(bw_bps), src, delay, dst, delay_add,
          delay_multiply, bw_add, bw_multiply, i, out);
  AddLink(Bandwidth::FromBitsPerSecond(bw_bps), dst, delay, src, delay_add,
          delay_multiply, bw_add, bw_multiply, i, out);
}

static void AddBiLink(Bandwidth bw, const std::string& src, Delay delay,
                      const std::string& dst, Delay delay_add,
                      double delay_multiply, size_t* i, PBNet* out) {
  AddLink(bw, src, delay, dst, delay_add, delay_multiply,
          Bandwidth::FromBitsPerSecond(0), 1.0, i, out);
  AddLink(bw, dst, delay, src, delay_add, delay_multiply,
          Bandwidth::FromBitsPerSecond(0), 1.0, i, out);
}

static void AddCluster(const std::string& name,
                       const std::vector<std::string>& ids, PBNet* out) {
  PBNetCluster* cluster = out->add_clusters();
  cluster->set_id(name);
  for (const std::string& id : ids) {
    cluster->add_nodes(id);
  }
}

PBNet GenerateHE(Bandwidth bw, Delay delay_add, double delay_multiply) {
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
  AddBiLink(bw, "HongKong", 0.09899999999999999, "LosAngeles", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "HongKong", 0.02197946590441512, "Singapore", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "HongKong", 0.024491937340699977, "Tokyo", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Tokyo", 0.045182269059776056, "Singapore", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "SanJose", 0.07082018626712423, "Tokyo", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Fremont", 0.00020944320149470966, "SanJose", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "PaloAlto", 0.0002098538430130341, "SanJose", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "PaloAlto", 0.00015330384758825228, "Fremont", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Portland", 0.007750976557599637, "SanJose", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Seattle", 0.00970358928467878, "SanJose", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Seattle", 0.001980189853586593, "Portland", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Seattle", 0.0016290555332783203, "Vancouver", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Denver", 0.013942438885259888, "Seattle", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "LosAngeles", 0.004379930992404047, "PaloAlto", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Phoenix", 0.004878751961226585, "LosAngeles", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "LasVegas", 0.003127585787117664, "LosAngeles", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Denver", 0.008271265381124625, "LasVegas", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Denver", 0.012679134906052427, "SanJose", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Denver", 0.009549009601746527, "Minneapolis", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Denver", 0.00761386951297863, "KansasCity", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Dallas", 0.012085544227624674, "Phoenix", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Dallas", 0.019896482083487595, "Fremont", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Dallas", 0.006205167979753415, "KansasCity", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Dallas", 0.010971931036320862, "Chicago", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Dallas", 0.01588028050776086, "Ashburn", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Dallas", 0.009845557514101322, "Atlanta", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Dallas", 0.015180864669691761, "Miami", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Miami", 0.00827928825214428, "Atlanta", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Ashburn", 0.007244818676274683, "Atlanta", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "KansasCity", 0.005613545439041082, "Chicago", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "NewYork", 0.009734695650495928, "Chicago", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Toronto", 0.0059679334825478835, "Chicago", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Minneapolis", 0.004854188292269742, "Chicago", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Ashburn", 0.0029763178724357656, "NewYork", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Toronto", 0.004719392909665924, "NewYork", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "PaloAlto", 0.03499418206738756, "NewYork", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "SanJose", 0.03485362282421041, "NewYork", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Ashburn", 0.03268078197594825, "SanJose", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "London", 0.04732289625262841, "NewYork", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Ashburn", 0.050272198201872294, "London", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Amsterdam", 0.049801303976444715, "NewYork", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Ashburn", 0.052553436093461164, "Paris", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "London", 0.0029205763165338866, "Paris", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Zurich", 0.004154523412587286, "Paris", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Frankfurt am Main", 0.004065115464977358, "Paris", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Amsterdam", 0.003657458411802766, "Paris", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Amsterdam", 0.0030323776775934013, "London", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Frankfurt am Main", 0.005416362162592938, "London", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Amsterdam", 0.009561504025009172, "Stockholm", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Amsterdam", 0.0049050665652040255, "Berlin", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Amsterdam", 0.0030944299935970203, "Frankfurt am Main",
            delay_add, delay_multiply, &i, &out);
  AddBiLink(bw, "Amsterdam", 0.005227380278661229, "Zurich", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Prague", 0.0034757563350856072, "Frankfurt am Main", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Zurich", 0.0025991945628872828, "Frankfurt am Main", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Warsaw", 0.004395690813733776, "Prague", delay_add,
            delay_multiply, &i, &out);
  AddBiLink(bw, "Berlin", 0.004390355474006233, "Warsaw", delay_add,
            delay_multiply, &i, &out);

  return out;
}

PBNet GenerateNTT(Delay delay_add, double delay_multiply, Bandwidth bw_add,
                  double bw_multiply) {
  PBNet out;

  AddCluster("Asia", {"osaka", "kualalumpur", "hongkong", "brunei", "seoul",
                      "singapore", "bangkok", "tokyo", "jakarta", "taipei"},
             &out);
  AddCluster("North America",
             {"houston", "sanjose", "seattle", "LA", "newyork", "dc", "boston",
              "atlanta", "miami", "chicago", "dallas"},
             &out);
  AddCluster("South America", {"saopaulo"}, &out);
  AddCluster("Europe",
             {"bucharest", "madrid", "sofia", "milan", "london", "amsterdam",
              "warsaw", "luxembourg", "valencia", "stockholm", "munich",
              "berlin", "brussels", "budapest", "marseille", "dusseldorf",
              "barcelona", "paris", "vienna", "frankfurt"},
             &out);
  AddCluster("Australia", {"sydney"}, &out);

  size_t i = 1;
  AddBiLink(400000000000ul, "tokyo", 0.0440402438126, "LA", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(170000000000ul, "tokyo", 0.0384675854525, "seattle", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(320000000000ul, "tokyo", 0.0416617032847, "sanjose", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(660000000000ul, "osaka", 0.00200511038863, "tokyo", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(20000000000ul, "tokyo", 0.0391165806529, "sydney", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(260000000000ul, "tokyo", 0.0144204243915, "hongkong", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(80000000000ul, "tokyo", 0.0104964514544, "taipei", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(140000000000ul, "tokyo", 0.0265926970908, "singapore", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(10000000000ul, "tokyo", 0.00579455563354, "seoul", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(80000000000ul, "osaka", 0.0402090563017, "seattle", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(140000000000ul, "osaka", 0.043534645896, "sanjose", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(370000000000ul, "osaka", 0.0459271862836, "LA", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(100000000000ul, "osaka", 0.0247606172858, "singapore", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(290000000000ul, "osaka", 0.0124464408122, "hongkong", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(10000000000ul, "osaka", 0.00414594196265, "seoul", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(60000000000ul, "osaka", 0.00857148496165, "taipei", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(20000000000ul, "hongkong", 0.00407591702825, "taipei", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(110000000000ul, "hongkong", 0.0129549377611, "singapore", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(70000000000ul, "singapore", 0.0542395406027, "london", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(20000000000ul, "singapore", 0.00154630285189, "kualalumpur",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "singapore", 0.0682342201114, "sanjose", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(20000000000ul, "sydney", 0.0602720584442, "LA", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(220000000000ul, "amsterdam", 0.00182209284912, "frankfurt",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(10000000000ul, "amsterdam", 0.000909833341748, "dusseldorf",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(10000000000ul, "amsterdam", 0.000869200417323, "brussels",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(180000000000ul, "amsterdam", 0.00414870408579, "milan", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(300000000000ul, "frankfurt", 0.00318691684306, "london", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(10000000000ul, "frankfurt", 0.000913053085595, "dusseldorf",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(30000000000ul, "frankfurt", 0.00445101062351, "warsaw", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(610000000000ul, "frankfurt", 0.00726475139602, "bucharest",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(130000000000ul, "frankfurt", 0.00405812441165, "budapest",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(40000000000ul, "frankfurt", 0.00694408831625, "sofia", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(20000000000ul, "frankfurt", 0.00152045524726, "munich", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(210000000000ul, "frankfurt", 0.00259092434959, "milan", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(490000000000ul, "london", 0.00178661083351, "amsterdam", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(10000000000ul, "london", 0.0016034321954, "brussels", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(160000000000ul, "madrid", 0.00631305134948, "london", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(120000000000ul, "madrid", 0.00740711873213, "amsterdam", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(130000000000ul, "paris", 0.00171381959824, "london", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "paris", 0.00215062165847, "amsterdam", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(400000000000ul, "newyork", 0.0278523715932, "london", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "newyork", 0.0310121935076, "frankfurt", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(400000000000ul, "dc", 0.0309484781572, "amsterdam", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "dc", 0.0326528976101, "frankfurt", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(400000000000ul, "seattle", 0.00571088101251, "sanjose", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "seattle", 0.013944272859, "chicago", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "seattle", 0.0193272573202, "newyork", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "sanjose", 0.0147912336407, "chicago", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(400000000000ul, "sanjose", 0.0194414156732, "dc", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(400000000000ul, "sanjose", 0.00246677083724, "LA", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(400000000000ul, "LA", 0.0100216743653, "dallas", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "dallas", 0.00647050831368, "chicago", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(30000000000ul, "dallas", 0.00182148583442, "houston", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "dallas", 0.0116488251407, "sanjose", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "dallas", 0.00579917359141, "atlanta", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "dallas", 0.00951351743117, "dc", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "chicago", 0.00572131169056, "newyork", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "miami", 0.00894378578209, "dallas", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(400000000000ul, "dc", 0.00744267357058, "miami", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(400000000000ul, "dc", 0.00164245029422, "newyork", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "dc", 0.00435963693841, "atlanta", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "dc", 0.0185521687536, "LA", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "atlanta", 0.00487263179876, "miami", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(30000000000ul, "newyork", 0.0384269996315, "saopaulo", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(30000000000ul, "miami", 0.032832730137, "saopaulo", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(120000000000ul, "amsterdam", 0.00560044467656, "stockholm",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(260000000000ul, "frankfurt", 0.00298833201945, "vienna", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(100000000000ul, "frankfurt", 0.00591448704821, "stockholm",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(40000000000ul, "osaka", 0.0247445962548, "kualalumpur", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(30000000000ul, "barcelona", 0.00252419850939, "madrid", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(40000000000ul, "barcelona", 0.00619614168801, "amsterdam",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(20000000000ul, "singapore", 0.00452808193141, "jakarta", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(10000000000ul, "singapore", 0.00630897776255, "brunei", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(20000000000ul, "hongkong", 0.0099359167697, "brunei", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(20000000000ul, "amsterdam", 0.00731156034184, "valencia", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(180000000000ul, "madrid", 0.00134688070725, "valencia", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(100000000000ul, "luxembourg", 0.0013869784069, "amsterdam",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "luxembourg", 0.000880679729202, "frankfurt",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(6000000000ul, "singapore", 0.0071197805129, "bangkok", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(15000000000ul, "kualalumpur", 0.0059220982258, "bangkok", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(20000000000ul, "osaka", 0.0389890992677, "sydney", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(100000000000ul, "boston", 0.0015962339139, "newyork", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(100000000000ul, "boston", 0.00680157749372, "chicago", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(200000000000ul, "dc", 0.00477882625241, "chicago", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(100000000000ul, "marseille", 0.00504988093448, "amsterdam",
            delay_add, delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(100000000000ul, "marseille", 0.003992628199, "frankfurt", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(10000000000ul, "berlin", 0.00288467585939, "amsterdam", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);
  AddBiLink(10000000000ul, "berlin", 0.00211788639232, "frankfurt", delay_add,
            delay_multiply, bw_add, bw_multiply, &i, &out);

  return out;
}

net::PBNet GenerateFullGraph(uint32_t size, Bandwidth bw, Delay delay) {
  std::mt19937 rnd(1.0);
  return GenerateRandom(size, 1.0, delay, delay, bw, bw, &rnd);
}

net::PBNet GenerateRandom(size_t n, double edge_prob, Delay delay_min,
                          Delay delay_max, Bandwidth bw_min, Bandwidth bw_max,
                          std::mt19937* generator) {
  CHECK(delay_min <= delay_max);
  CHECK(bw_min.bps() <= bw_max.bps());
  if (n == 0) {
    return {};
  }

  std::vector<size_t> ids(n);
  std::iota(ids.begin(), ids.end(), 0);
  std::shuffle(ids.begin(), ids.end(), *generator);

  auto delay_dist = std::uniform_int_distribution<uint64_t>(delay_min.count(),
                                                            delay_max.count());
  auto bw_dist =
      std::uniform_int_distribution<uint64_t>(bw_min.bps(), bw_max.bps());
  auto edge_add_dist = std::uniform_real_distribution<double>(0.0, 1.0);

  // Add links between ids, then add randomly pick pairs and add links.
  std::set<std::pair<size_t, size_t>> added;
  for (size_t i = 0; i < n - 1; ++i) {
    size_t src = ids[i];
    size_t dst = ids[i + 1];
    if (src > dst) {
      std::swap(src, dst);
    }

    added.emplace(src, dst);
  }

  for (uint32_t i = 0; i < n; ++i) {
    for (uint32_t j = 0; j < n; ++j) {
      size_t src = i;
      size_t dst = j;
      if (src >= dst) {
        continue;
      }

      double p = edge_add_dist(*generator);
      if (p > edge_prob) {
        continue;
      }

      added.emplace(src, dst);
    }
  }

  net::PBNet return_graph;
  for (const auto& src_and_dst : added) {
    std::string src = "N" + std::to_string(src_and_dst.first);
    std::string dst = "N" + std::to_string(src_and_dst.second);

    std::chrono::microseconds delay =
        std::chrono::microseconds(delay_dist(*generator));
    uint64_t bw_bps = bw_dist(*generator);
    AddBiEdgeToGraph(src, dst, delay, Bandwidth::FromBitsPerSecond(bw_bps),
                     &return_graph);
  }

  return return_graph;
}

net::PBNet GenerateLadder(size_t levels, Bandwidth bw, Delay delay,
                          const std::vector<microseconds>& central_delays) {
  net::PBNet return_graph;
  CHECK(levels > 0) << "Tried to generate ladder with no edges,";

  if (!central_delays.empty()) {
    CHECK(central_delays.size() == levels);
  }

  size_t port_gen = 1;
  for (size_t level = 0; level < levels; ++level) {
    std::string node_left = StrCat("N", std::to_string(4 * level));
    std::string node_right = StrCat("N", std::to_string(4 * level + 1));

    microseconds central_link_delay = delay;
    if (!central_delays.empty()) {
      central_link_delay = central_delays[level];
    }

    AddBiLink(bw, node_left, central_link_delay, node_right,
              microseconds::zero(), 1.0, &port_gen, &return_graph);

    if (level != 0) {
      std::string mid_node_left =
          StrCat("N", std::to_string(4 * (level - 1) + 2));

      std::string mid_node_right =
          StrCat("N", std::to_string(4 * (level - 1) + 3));

      std::string prev_node_left = StrCat("N", std::to_string(4 * (level - 1)));
      std::string prev_node_right =
          StrCat("N", std::to_string(4 * (level - 1) + 1));

      AddBiLink(bw, node_left, delay, mid_node_left, microseconds::zero(), 1.0,
                &port_gen, &return_graph);
      AddBiLink(bw, mid_node_left, delay, prev_node_left, microseconds::zero(),
                1.0, &port_gen, &return_graph);

      AddBiLink(bw, node_right, delay, mid_node_right, microseconds::zero(),
                1.0, &port_gen, &return_graph);
      AddBiLink(bw, mid_node_right, delay, prev_node_right,
                microseconds::zero(), 1.0, &port_gen, &return_graph);
    }
  }

  return return_graph;
}

net::PBNet GenerateBraess(Bandwidth bw) {
  PBNet net;
  AddEdgeToGraph("C", "D", milliseconds(10), bw, &net);
  AddEdgeToGraph("D", "C", milliseconds(10), bw, &net);
  AddEdgeToGraph("B", "D", milliseconds(8), bw, &net);
  AddEdgeToGraph("D", "B", milliseconds(8), bw, &net);
  AddEdgeToGraph("A", "B", milliseconds(10), bw, &net);
  AddEdgeToGraph("B", "A", milliseconds(10), bw, &net);
  AddEdgeToGraph("A", "C", milliseconds(5), bw, &net);
  AddEdgeToGraph("C", "A", milliseconds(5), bw, &net);
  AddEdgeToGraph("B", "C", milliseconds(1), bw, &net);

  return net;
}

PBNet GenerateSprint(Bandwidth bw, Delay delay_add, double delay_multiply) {
  PBNet out;
  size_t i = 1;

  AddCluster("Asia", {"Hong+Kong4042", "Hong+Kong6421", "Singapore4094",
                      "Tokyo4069", "Tokyo4070", "Tokyo4071"},
             &out);
  AddCluster(
      "North America",
      {"Anaheim,+CA4031", "Anaheim,+CA4073", "Anaheim,+CA4099",
       "Anaheim,+CA4100", "Anaheim,+CA4101", "Anaheim,+CA6438",
       "Anaheim,+CA6453", "Anaheim,+CA6464", "Anaheim,+CA6490",
       "Anaheim,+CA6502", "Anaheim,+CA6512", "Anaheim,+CA6520",
       "Anaheim,+CA6539", "Anaheim,+CA6556", "Anaheim,+CA6564",
       "Anaheim,+CA6570", "Anaheim,+CA6578", "Anaheim,+CA6584",
       "Anaheim,+CA6591", "Anaheim,+CA6610", "Anaheim,+CA6627",
       "Anaheim,+CA6684", "Anaheim,+CA6702", "Anaheim,+CA6721",
       "Anaheim,+CA6744", "Ashburn,+VA10161", "Ashburn,+VA10162",
       "Ashburn,+VA10163", "Atlanta,+GA2338", "Atlanta,+GA2344",
       "Atlanta,+GA2347", "Atlanta,+GA3957", "Atlanta,+GA4032",
       "Atlanta,+GA4074", "Atlanta,+GA4102", "Atlanta,+GA6439",
       "Atlanta,+GA6465", "Atlanta,+GA6481", "Atlanta,+GA6530",
       "Atlanta,+GA6540", "Atlanta,+GA6565", "Atlanta,+GA6628",
       "Atlanta,+GA6685", "Atlanta,+GA6735", "Atlanta,+GA6745", "Boston5499",
       "Boston5509", "Cheyenne,+WY2439", "Cheyenne,+WY4034", "Cheyenne,+WY4076",
       "Cheyenne,+WY6388", "Cheyenne,+WY6413", "Cheyenne,+WY6455",
       "Cheyenne,+WY6542", "Cheyenne,+WY6629", "Cheyenne,+WY6723",
       "Cheyenne,+WY6746", "Chicago,+IL1391", "Chicago,+IL1484",
       "Chicago,+IL4036", "Chicago,+IL4037", "Chicago,+IL4104",
       "Chicago,+IL4122", "Chicago,+IL4123", "Chicago,+IL6414",
       "Chicago,+IL6441", "Chicago,+IL6468", "Chicago,+IL6492",
       "Chicago,+IL6531", "Chicago,+IL6572", "Chicago,+IL6586",
       "Chicago,+IL6593", "Chicago,+IL6595", "Chicago,+IL6603",
       "Chicago,+IL6611", "Chicago,+IL6621", "Chicago,+IL6639",
       "Chicago,+IL6643", "Chicago,+IL6644", "Chicago,+IL6648",
       "Chicago,+IL6651", "Chicago,+IL6652", "Chicago,+IL6654",
       "Chicago,+IL6669", "Chicago,+IL6724", "Chicago,+IL6747",
       "Dallas,+TX1742", "Dallas,+TX2635", "Dallas,+TX3549", "Dallas,+TX4015",
       "Dallas,+TX4080", "Dallas,+TX4115", "Dallas,+TX4133", "Dallas,+TX6419",
       "Dallas,+TX6444", "Dallas,+TX6471", "Dallas,+TX6483", "Dallas,+TX6494",
       "Dallas,+TX6504", "Dallas,+TX6558", "Dallas,+TX6566", "Dallas,+TX6573",
       "Dallas,+TX6580", "Dallas,+TX6587", "Dallas,+TX6598", "Dallas,+TX6604",
       "Dallas,+TX6612", "Dallas,+TX6622", "Dallas,+TX6641", "Dallas,+TX6645",
       "Dallas,+TX6658", "Dallas,+TX6663", "Dallas,+TX6683", "Dallas,+TX6689",
       "Dallas,+TX6706", "Dallas,+TX6726", "Dallas,+TX6737", "Dallas,+TX6749",
       "Denver,+Colorado5501", "Denver,+Colorado5511", "Kansas+City,+MO4043",
       "Kansas+City,+MO4082", "Kansas+City,+MO4105", "Kansas+City,+MO4106",
       "Kansas+City,+MO6395", "Kansas+City,+MO6422", "Kansas+City,+MO6458",
       "Kansas+City,+MO6472", "Kansas+City,+MO6544", "Kansas+City,+MO6690",
       "Kansas+City,+MO6707", "Kansas+City,+MO6750", "Lees+Summit,+MO5503",
       "Lees+Summit,+MO5504", "Lees+Summit,+MO5505", "Lees+Summit,+MO5512",
       "Los+Angeles,+CA5502", "Manasquan,+NJ4047", "Manasquan,+NJ4086",
       "New+York,+NY4017", "New+York,+NY4022", "New+York,+NY4024",
       "New+York,+NY4028", "New+York,+NY4048", "New+York,+NY4088",
       "New+York,+NY4107", "New+York,+NY4116", "New+York,+NY4124",
       "New+York,+NY4129", "New+York,+NY4134", "New+York,+NY4137",
       "New+York,+NY6397", "New+York,+NY6427", "New+York,+NY6446",
       "New+York,+NY6496", "New+York,+NY6524", "New+York,+NY6532",
       "New+York,+NY6559", "New+York,+NY6605", "New+York,+NY6606",
       "New+York,+NY6751", "New+York,+NY6752", "Orlando,+FL4049",
       "Orlando,+FL4050", "Orlando,+FL4089", "Orlando,+FL4108",
       "Orlando,+FL6429", "Orlando,+FL6459", "Orlando,+FL6693",
       "Pearl+Harbor,+HI4053", "Pearl+Harbor,+HI4092", "Pearl+Harbor,+HI6400",
       "Pearl+Harbor,+HI6550", "Pennsauken,+NJ4052", "Pennsauken,+NJ4091",
       "Pennsauken,+NJ4109", "Pennsauken,+NJ4117", "Pennsauken,+NJ4125",
       "Pennsauken,+NJ4126", "Pennsauken,+NJ4135", "Pennsauken,+NJ6399",
       "Pennsauken,+NJ6448", "Pennsauken,+NJ6486", "Pennsauken,+NJ6516",
       "Pennsauken,+NJ6517", "Pennsauken,+NJ6525", "Pennsauken,+NJ6614",
       "Pennsauken,+NJ6624", "Pennsauken,+NJ6647", "Pennsauken,+NJ6728",
       "Pennsauken,+NJ6754", "Rancho+Cordova,+CA5507", "Rancho+Cordova,+CA5514",
       "Relay,+MD4029", "Relay,+MD4054", "Relay,+MD4093", "Relay,+MD4110",
       "Relay,+MD4118", "Relay,+MD4127", "Relay,+MD4131", "Relay,+MD4136",
       "Relay,+MD4138", "Relay,+MD5801", "Relay,+MD6431", "Relay,+MD6449",
       "Relay,+MD6461", "Relay,+MD6487", "Relay,+MD6518", "Relay,+MD6551",
       "Relay,+MD6576", "Relay,+MD6675", "Relay,+MD6696",
       "Research+Triangle+Park,+NC4057", "Research+Triangle+Park,+NC4058",
       "Reston,+VA5496", "Reston,+VA5497", "Richardson,+TX5500",
       "Roachdale,+IN4056", "Roachdale,+IN4111", "Roachdale,+IN6402",
       "Roachdale,+IN6552", "Roachdale,+IN6636", "Roachdale,+IN6676",
       "Roachdale,+IN6677", "Roachdale,+IN6697", "Roachdale,+IN6712",
       "Roachdale,+IN6729", "San+Jose,+CA4062", "San+Jose,+CA4095",
       "San+Jose,+CA4112", "San+Jose,+CA4119", "San+Jose,+CA4132",
       "San+Jose,+CA6405", "San+Jose,+CA6477", "San+Jose,+CA6742",
       "Santa+Clara,+CA5508", "Seattle,+WA4019", "Seattle,+WA4059",
       "Seattle,+WA4060", "Seattle,+WA6432", "Seattle,+WA6450",
       "Seattle,+WA6476", "Springfield,+MA4020", "Springfield,+MA4023",
       "Springfield,+MA4025", "Springfield,+MA6406", "Stockton,+CA3402",
       "Stockton,+CA4064", "Stockton,+CA4065", "Stockton,+CA4096",
       "Stockton,+CA4113", "Stockton,+CA6407", "Stockton,+CA6452",
       "Stockton,+CA6463", "Stockton,+CA6479", "Stockton,+CA6563",
       "Stockton,+CA6569", "Stockton,+CA6577", "Stockton,+CA6583",
       "Stockton,+CA6590", "Stockton,+CA6602", "Stockton,+CA6609",
       "Stockton,+CA6619", "Stockton,+CA6719", "Stockton,+CA6758",
       "Tacoma,+WA3251", "Tacoma,+WA4114", "Tacoma,+WA6408", "Tacoma,+WA6555",
       "Tacoma,+WA6701", "Tacoma,+WA6720", "Washington,+DC4139",
       "Washington,+DC4140", "Washington,+DC4142", "Washington,+DC6393",
       "Washington,+DC6415", "Washington,+DC6443", "Washington,+DC6456",
       "Washington,+DC6469", "Washington,+DC6543", "Washington,+DC6736",
       "Washington,+DC6748", "Washington,+DC9643"},
      &out);

  AddCluster("Europe",
             {"Amsterdam4030", "Amsterdam4072", "Brussels,+Belgium4033",
              "Brussels,+Belgium4075", "Copenhagen4038", "Copenhagen4077",
              "Dublin,+Ireland4039", "Dublin,+Ireland4078", "Frankfurt4040",
              "Frankfurt4079", "Hamburg,+Germany4041", "Hamburg,+Germany4081",
              "London4044", "London4045", "London4083", "London4084",
              "Milan,+Italy4046", "Milan,+Italy4085", "Munich,+Germany4087",
              "Paris4051", "Paris4090", "Stockholm,+Sweden4066",
              "Stockholm,+Sweden4097"},
             &out);

  AddCluster("Australia", {"Sydney,+Australia4067", "Sydney,+Australia4068",
                           "Sydney,+Australia6437"},
             &out);
  AddLink(bw, "San+Jose,+CA4062", 0.004, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4062", 0.001, "San+Jose,+CA4119", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4062", 0.007, "Tacoma,+WA3251", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4062", 0.021, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4062", 0.004, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4062", 0.001, "San+Jose,+CA4112", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4062", 0.001, "San+Jose,+CA6742", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4062", 0.001, "San+Jose,+CA4095", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4062", 0.001, "San+Jose,+CA6405", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4062", 0.001, "San+Jose,+CA4132", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4062", 0.016, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6490", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6490", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Manasquan,+NJ4086", 0.002, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Manasquan,+NJ4086", 0.033, "Copenhagen4038", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Manasquan,+NJ4086", 0.001, "Manasquan,+NJ4047", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Manasquan,+NJ4086", 0.003, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6564", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6564", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6564", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6564", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6564", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6564", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6566", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6566", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6566", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6566", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6566", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6456", 0.001, "Washington,+DC6736", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6456", 0.001, "Washington,+DC6543", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6456", 0.001, "Washington,+DC6415", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6456", 0.002, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6456", 0.002, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6456", 0.001, "Washington,+DC6393", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6456", 0.002, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA6720", 0.001, "Tacoma,+WA3251", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6494", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6494", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6494", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6494", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6494", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6712", 0.001, "Roachdale,+IN4111", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6712", 0.001, "Roachdale,+IN4056", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "London4083", 0.001, "London4044", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "London4083", 0.003, "Brussels,+Belgium4033", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "London4083", 0.03, "Manasquan,+NJ4047", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "London4083", 0.004, "Dublin,+Ireland4078", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "London4083", 0.029, "New+York,+NY4022", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "London4084", 0.029, "New+York,+NY4024", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "London4084", 0.003, "Brussels,+Belgium4033", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "London4084", 0.03, "Manasquan,+NJ4047", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "London4084", 0.029, "New+York,+NY4022", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Research+Triangle+Park,+NC4057", 0.004, "Relay,+MD4054",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Milan,+Italy4046", 0.001, "Milan,+Italy4085", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Milan,+Italy4046", 0.005, "Paris4090", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4092", 0.022, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4092", 0.001, "Pearl+Harbor,+HI6400", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4092", 0.001, "Pearl+Harbor,+HI4053", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4092", 0.021, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4092", 0.001, "Pearl+Harbor,+HI6550", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4092", 0.022, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Research+Triangle+Park,+NC4058", 0.004, "Atlanta,+GA4074",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA4114", 0.007, "San+Jose,+CA4095", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA4114", 0.001, "Tacoma,+WA6408", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA4114", 0.001, "Tacoma,+WA3251", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA4114", 0.009, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA4114", 0.001, "Tacoma,+WA6555", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA4114", 0.04, "Tokyo4070", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Hamburg,+Germany4041", 0.003, "Copenhagen4038", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Hamburg,+Germany4041", 0.003, "Amsterdam4072", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Hamburg,+Germany4041", 0.001, "Hamburg,+Germany4081", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Hamburg,+Germany4041", 0.005, "Munich,+Germany4087", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6565", 0.001, "Atlanta,+GA3957", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6565", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6551", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6551", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6551", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Denver,+Colorado5511", 0.002, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6624", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6624", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6624", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Lees+Summit,+MO5512", 0.002, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Lees+Summit,+MO5512", 0.002, "Kansas+City,+MO4106", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6746", 0.006, "Kansas+City,+MO6544", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Rancho+Cordova,+CA5507", 0.001, "Rancho+Cordova,+CA5514",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Rancho+Cordova,+CA5507", 0.002, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Rancho+Cordova,+CA5507", 0.002, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6487", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6486", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6486", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6486", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6641", 0.007, "Atlanta,+GA3957", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6641", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6641", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6641", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6641", 0.008, "Chicago,+IL6593", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6641", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6641", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.004, "San+Jose,+CA4119", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6464", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6570", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6627", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6539", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.022, "Pearl+Harbor,+HI4053", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6556", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6564", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6744", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6591", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.011, "Dallas,+TX4133", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6584", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.022, "Pearl+Harbor,+HI4092", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.011, "Dallas,+TX6587", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6684", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.011, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6502", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6520", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6512", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.004, "San+Jose,+CA4112", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6702", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6453", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4100", 0.001, "Anaheim,+CA6438", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.004, "San+Jose,+CA4062", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6464", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6627", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6490", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.022, "Pearl+Harbor,+HI4053", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6539", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6564", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6556", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6744", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6591", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6584", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6684", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.011, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.011, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6502", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6520", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6512", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6702", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6453", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA6438", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4101", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6707", 0.001, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6707", 0.001, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6707", 0.001, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6707", 0.001, "Kansas+City,+MO4106", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6570", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6570", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6645", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6645", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6645", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6645", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6645", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6464", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6570", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6627", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6539", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6556", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6564", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.021, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.011, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6744", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.002, "Los+Angeles,+CA5502", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6578", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6684", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6502", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6512", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6520", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.004, "San+Jose,+CA4112", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6702", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6453", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA6438", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4031", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6573", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6573", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6573", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6573", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6573", 0.001, "Dallas,+TX6558", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6573", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6573", 0.011, "Relay,+MD5801", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6393", 0.001, "Washington,+DC6443", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6393", 0.001, "Washington,+DC9643", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6393", 0.001, "Washington,+DC6469", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6393", 0.001, "Washington,+DC4139", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6393", 0.001, "Washington,+DC4140", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6393", 0.001, "Washington,+DC6456", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6393", 0.001, "Washington,+DC6748", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6393", 0.001, "Washington,+DC4142", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6492", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6492", 0.001, "Chicago,+IL4122", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6492", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6492", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6492", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6492", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6639", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6639", 0.001, "Chicago,+IL4122", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6639", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6639", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6639", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6639", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6578", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6469", 0.001, "Washington,+DC6415", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6469", 0.002, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6469", 0.002, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6469", 0.001, "Washington,+DC6393", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6469", 0.002, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4111", 0.007, "Pennsauken,+NJ4117", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4111", 0.001, "Roachdale,+IN6729", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4111", 0.001, "Roachdale,+IN6676", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4111", 0.001, "Roachdale,+IN4056", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4111", 0.003, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4111", 0.001, "Roachdale,+IN6712", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4111", 0.001, "Roachdale,+IN6552", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4111", 0.001, "Roachdale,+IN6697", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4111", 0.001, "Roachdale,+IN6402", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4111", 0.001, "Roachdale,+IN6636", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4111", 0.003, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.005, "Orlando,+FL4049", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA6735", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA6745", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA2344", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA6685", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA2338", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA6540", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.005, "Orlando,+FL4050", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA2347", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.006, "Relay,+MD4131", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA6439", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA6465", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA6481", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4102", 0.001, "Atlanta,+GA6628", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6729", 0.001, "Roachdale,+IN4111", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6729", 0.001, "Roachdale,+IN4056", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.006, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA6540", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.008, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.006, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA6481", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA6465", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA6628", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA6565", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.007, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA6735", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA3957", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA6745", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA2344", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA6685", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA2338", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA2347", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.005, "Orlando,+FL4108", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4032", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA3251", 0.007, "San+Jose,+CA4062", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA3251", 0.001, "Tacoma,+WA6408", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA3251", 0.009, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA3251", 0.001, "Tacoma,+WA6701", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA3251", 0.002, "Seattle,+WA4059", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA3251", 0.001, "Tacoma,+WA6720", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA3251", 0.064, "Sydney,+Australia4067", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA3251", 0.009, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA3251", 0.001, "Tacoma,+WA6555", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA3251", 0.002, "Seattle,+WA4060", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA3251", 0.001, "Tacoma,+WA4114", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA3251", 0.04, "Tokyo4070", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.009, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6724", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6643", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6572", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6492", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6654", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6468", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6639", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL4122", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6648", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6747", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6593", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.008, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6586", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6595", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6669", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.007, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.008, "Springfield,+MA4023", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.009, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6414", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.006, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6441", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6603", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6611", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6531", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1484", 0.001, "Chicago,+IL6621", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL6459", 0.001, "Orlando,+FL4049", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL6459", 0.001, "Orlando,+FL4050", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6427", 0.001, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6427", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6427", 0.001, "New+York,+NY4107", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6427", 0.001, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6427", 0.001, "New+York,+NY6496", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Rancho+Cordova,+CA5514", 0.001, "Rancho+Cordova,+CA5507",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Rancho+Cordova,+CA5514", 0.002, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA6476", 0.001, "Seattle,+WA4019", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6464", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6627", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6539", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6556", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6564", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6744", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6684", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6502", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6520", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6512", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6702", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6453", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA6438", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6721", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4029", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4029", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.001, "Cheyenne,+WY6455", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.002, "Denver,+Colorado5501", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.001, "Cheyenne,+WY2439", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.001, "Cheyenne,+WY6723", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.009, "Tacoma,+WA3251", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.009, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.001, "Cheyenne,+WY6629", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.001, "Cheyenne,+WY6388", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.001, "Cheyenne,+WY6413", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.001, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.007, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.009, "Tacoma,+WA4114", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4076", 0.001, "Cheyenne,+WY6542", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6726", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6726", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6726", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6726", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6726", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6580", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6580", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6580", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6580", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6580", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6543", 0.001, "Washington,+DC6443", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6543", 0.001, "Washington,+DC4139", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6543", 0.001, "Washington,+DC4140", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6543", 0.001, "Washington,+DC6456", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6543", 0.001, "Washington,+DC4142", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6543", 0.001, "Washington,+DC6748", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Brussels,+Belgium4033", 0.003, "London4044", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Brussels,+Belgium4033", 0.003, "London4084", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Brussels,+Belgium4033", 0.001, "Brussels,+Belgium4075",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Brussels,+Belgium4033", 0.003, "London4083", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6643", 0.006, "Atlanta,+GA6735", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6643", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6643", 0.001, "Chicago,+IL6644", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6452", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6452", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6452", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6452", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.012, "Pennsauken,+NJ4135", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6580", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6726", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6645", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6483", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.011, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6573", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6566", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.005, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6494", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6737", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.009, "Orlando,+FL4089", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6587", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6749", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.008, "Chicago,+IL4123", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6598", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6689", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX1742", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.008, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6504", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX3549", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6604", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6612", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6419", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6622", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6444", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6641", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6706", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4115", 0.001, "Dallas,+TX6471", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6644", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6644", 0.001, "Chicago,+IL6643", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.001, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.01, "Pennsauken,+NJ4117", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.005, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.001, "Kansas+City,+MO6544", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.001, "Kansas+City,+MO6472", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.001, "Kansas+City,+MO6707", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.001, "Kansas+City,+MO6458", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.01, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.005, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.001, "Kansas+City,+MO6395", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.001, "Kansas+City,+MO6422", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.002, "Lees+Summit,+MO5512", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.001, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4105", 0.001, "Kansas+City,+MO6690", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6572", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6572", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4106", 0.001, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4106", 0.01, "Pennsauken,+NJ4117", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4106", 0.005, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4106", 0.001, "Kansas+City,+MO6544", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4106", 0.001, "Kansas+City,+MO6472", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4106", 0.001, "Kansas+City,+MO6707", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4106", 0.001, "Kansas+City,+MO6458", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4106", 0.001, "Kansas+City,+MO6395", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4106", 0.002, "Lees+Summit,+MO5512", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4106", 0.001, "Kansas+City,+MO6422", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4106", 0.001, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4106", 0.001, "Kansas+City,+MO6690", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.007, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.015, "Seattle,+WA4059", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6724", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6652", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.007, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6492", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6468", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.005, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6639", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL4122", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.003, "Roachdale,+IN4111", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6747", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6593", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.008, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6586", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6595", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6669", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.008, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.009, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6414", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6441", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6603", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6611", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6531", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4104", 0.001, "Chicago,+IL6621", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6658", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6658", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6584", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6584", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6584", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6648", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6648", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6587", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6587", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6587", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6587", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6587", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6587", 0.011, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA6405", 0.001, "San+Jose,+CA4062", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA6405", 0.001, "San+Jose,+CA4095", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6724", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6492", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6468", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6639", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL4122", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.003, "Roachdale,+IN4111", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.016, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6593", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6747", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.008, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6586", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6595", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6669", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.008, "Springfield,+MA4020", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.016, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6414", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6611", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6441", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6603", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6531", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4036", 0.001, "Chicago,+IL6621", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.016, "San+Jose,+CA4062", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.007, "New+York,+NY4124", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6724", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6644", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6572", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6654", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6492", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6468", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.005, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6639", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL4122", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6648", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6747", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6593", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.008, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6586", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6595", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6669", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.006, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.003, "Roachdale,+IN4056", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.009, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6414", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6603", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6441", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6611", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6531", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4037", 0.001, "Chicago,+IL6621", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4056", 0.007, "Pennsauken,+NJ4125", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4056", 0.001, "Roachdale,+IN6729", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4056", 0.007, "Pennsauken,+NJ4126", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4056", 0.001, "Roachdale,+IN6677", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4056", 0.001, "Roachdale,+IN6712", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4056", 0.001, "Roachdale,+IN6697", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4056", 0.001, "Roachdale,+IN6552", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4056", 0.001, "Roachdale,+IN6402", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4056", 0.001, "Roachdale,+IN4111", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4056", 0.001, "Roachdale,+IN6636", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN4056", 0.003, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6576", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6576", 0.001, "Relay,+MD4127", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4019", 0.001, "Seattle,+WA6476", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4019", 0.001, "Seattle,+WA4059", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4019", 0.001, "Seattle,+WA4060", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4019", 0.001, "Seattle,+WA6450", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6647", 0.001, "Pennsauken,+NJ4135", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6602", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6602", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ4117", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.002, "New+York,+NY4107", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.012, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ4135", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.002, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ6516", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ6614", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ6525", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ6517", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ6624", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ6448", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.021, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ6728", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ6486", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4109", 0.001, "Pennsauken,+NJ6754", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6724", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6724", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6724", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6724", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6724", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6663", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6663", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6651", 0.001, "Chicago,+IL4122", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Amsterdam4072", 0.001, "Amsterdam4030", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Amsterdam4072", 0.003, "Hamburg,+Germany4041", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6737", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6737", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6737", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6737", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6737", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6652", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6591", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6591", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6591", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6654", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6654", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6609", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6609", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6609", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6463", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6463", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6463", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6463", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4095", 0.001, "San+Jose,+CA4062", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4095", 0.001, "San+Jose,+CA4119", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4095", 0.002, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4095", 0.001, "San+Jose,+CA4112", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4095", 0.001, "San+Jose,+CA6477", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4095", 0.001, "San+Jose,+CA6405", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4095", 0.002, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4095", 0.004, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4095", 0.007, "Tacoma,+WA4114", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.001, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.001, "Kansas+City,+MO6544", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.005, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.001, "Kansas+City,+MO6472", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.001, "Kansas+City,+MO6707", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.001, "Kansas+City,+MO6458", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.001, "Kansas+City,+MO6750", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.001, "Kansas+City,+MO6395", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.001, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.001, "Kansas+City,+MO4106", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.005, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.002, "Lees+Summit,+MO5504", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.001, "Kansas+City,+MO6690", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.014, "Seattle,+WA4060", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4043", 0.005, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6598", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6598", 0.007, "Atlanta,+GA6735", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6598", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6598", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6598", 0.008, "Chicago,+IL6593", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6598", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6598", 0.005, "Kansas+City,+MO6458", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6598", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6586", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6586", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6586", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6586", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6586", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6735", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6735", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6735", 0.006, "Chicago,+IL6643", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6735", 0.007, "Dallas,+TX6598", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6735", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Hong+Kong4042", 0.001, "Hong+Kong6421", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Hong+Kong4042", 0.016, "Tokyo4069", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Hong+Kong4042", 0.016, "Tokyo4070", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Roachdale,+IN6676", 0.001, "Roachdale,+IN4111", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6677", 0.001, "Roachdale,+IN4056", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.003, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.002, "Washington,+DC6443", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD4136", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.021, "San+Jose,+CA4062", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.002, "Washington,+DC9643", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.002, "Washington,+DC6469", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD6461", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD6518", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD4138", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.006, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD4131", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.003, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD6551", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.002, "Washington,+DC4139", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.003, "Manasquan,+NJ4086", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD6431", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.002, "Washington,+DC4140", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD6487", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD6696", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.002, "Washington,+DC6456", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD6675", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD4118", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD4029", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD6449", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.001, "Relay,+MD5801", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4110", 0.002, "Washington,+DC6748", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6446", 0.001, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6446", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6446", 0.001, "New+York,+NY4107", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6446", 0.001, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6446", 0.001, "New+York,+NY6496", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6728", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6728", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6728", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4118", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4118", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4118", 0.002, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4118", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4118", 0.001, "Relay,+MD4127", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dublin,+Ireland4078", 0.004, "London4083", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4117", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4117", 0.001, "Pennsauken,+NJ4126", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4117", 0.001, "Pennsauken,+NJ4135", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4117", 0.007, "Roachdale,+IN4111", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4117", 0.01, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4117", 0.01, "Kansas+City,+MO4106", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4117", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Sydney,+Australia6437", 0.001, "Sydney,+Australia4067",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Sydney,+Australia6437", 0.001, "Sydney,+Australia4068",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6744", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6744", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6744", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6744", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6744", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6744", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Los+Angeles,+CA5502", 0.002, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6413", 0.001, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6413", 0.001, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6413", 0.011, "Atlanta,+GA6530", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4133", 0.011, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4133", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4133", 0.011, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4122", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4122", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4122", 0.001, "Chicago,+IL6651", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4122", 0.008, "Springfield,+MA4025", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4122", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4122", 0.001, "Chicago,+IL6492", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4122", 0.001, "Chicago,+IL6639", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4122", 0.001, "Chicago,+IL6414", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4122", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4122", 0.001, "Chicago,+IL6611", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4122", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4122", 0.001, "Chicago,+IL6621", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6749", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6749", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6749", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6749", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6619", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6619", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6619", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL4123", 0.008, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6593", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6593", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6593", 0.008, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6593", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6593", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6593", 0.008, "Dallas,+TX6598", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6593", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6593", 0.008, "Dallas,+TX6641", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6595", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6595", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6595", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6595", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6595", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6669", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6669", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6669", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6669", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6669", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6479", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6479", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6479", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6479", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Milan,+Italy4085", 0.001, "Milan,+Italy4046", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Milan,+Italy4085", 0.004, "Frankfurt4040", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6745", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6745", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6745", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Hamburg,+Germany4081", 0.003, "Copenhagen4077", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Hamburg,+Germany4081", 0.001, "Hamburg,+Germany4041", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Hamburg,+Germany4081", 0.004, "Frankfurt4040", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6524", 0.001, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6524", 0.001, "New+York,+NY4129", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.002, "Washington,+DC6443", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD4136", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.002, "Reston,+VA5496", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.003, "New+York,+NY4107", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD4138", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD6696", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.002, "Washington,+DC6456", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.003, "New+York,+NY4129", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.004, "Research+Triangle+Park,+NC4057",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.002, "Washington,+DC6469", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.002, "Washington,+DC4140", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD6431", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.002, "Washington,+DC6748", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD6461", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD6518", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.002, "Washington,+DC9643", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.002, "Washington,+DC4139", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD6551", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD6449", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD4131", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.003, "Manasquan,+NJ4047", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD4029", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD4118", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD6675", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4054", 0.001, "Relay,+MD4127", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4127", 0.001, "Relay,+MD4136", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4127", 0.001, "Relay,+MD4131", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4127", 0.001, "Relay,+MD6576", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4127", 0.002, "Ashburn,+VA10163", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4127", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4127", 0.001, "Relay,+MD4118", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4127", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ4126", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ6399", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.002, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.01, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ6516", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.021, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ6517", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ6525", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ6614", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ6624", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ6448", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ6728", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ6486", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4052", 0.001, "Pennsauken,+NJ6754", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4125", 0.003, "Springfield,+MA4020", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4125", 0.007, "Roachdale,+IN4056", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4125", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA6408", 0.001, "Tacoma,+WA3251", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA6408", 0.001, "Tacoma,+WA6555", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA6408", 0.001, "Tacoma,+WA4114", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4126", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4126", 0.001, "Pennsauken,+NJ4117", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4126", 0.003, "Springfield,+MA4020", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4126", 0.001, "Pennsauken,+NJ4135", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4126", 0.007, "Roachdale,+IN4056", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4126", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6683", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6683", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6684", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6684", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6684", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6684", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6684", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6747", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6747", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6747", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6747", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6747", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6464", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.011, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6627", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6539", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6556", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6564", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6744", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.004, "San+Jose,+CA4095", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6684", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6502", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6512", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6520", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6610", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6702", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6453", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4073", 0.001, "Anaheim,+CA6438", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6689", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6689", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6689", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6689", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6689", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX1742", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX1742", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX1742", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX1742", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX1742", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6605", 0.001, "New+York,+NY6606", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6605", 0.001, "New+York,+NY4124", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6605", 0.001, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6532", 0.001, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6532", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6532", 0.001, "New+York,+NY4107", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6532", 0.001, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6685", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6685", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6685", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6606", 0.001, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6606", 0.001, "New+York,+NY4124", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6606", 0.001, "New+York,+NY6605", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6697", 0.001, "Roachdale,+IN4111", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6697", 0.001, "Roachdale,+IN4056", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6402", 0.001, "Roachdale,+IN4111", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6402", 0.001, "Roachdale,+IN4056", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.005, "Orlando,+FL4049", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.007, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA6540", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.004, "Research+Triangle+Park,+NC4058",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA6465", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA6481", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA6628", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.008, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA6735", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA3957", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA6745", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.006, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA2344", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.006, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA6685", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA2338", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.001, "Atlanta,+GA2347", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA4074", 0.005, "Orlando,+FL4050", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4131", 0.001, "Relay,+MD4136", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4131", 0.003, "New+York,+NY6397", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4131", 0.002, "Ashburn,+VA10162", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4131", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4131", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4131", 0.006, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4131", 0.001, "Relay,+MD4127", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4131", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Paris4051", 0.001, "Paris4090", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "Paris4051", 0.004, "Frankfurt4079", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Relay,+MD6675", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6675", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6675", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4136", 0.001, "Relay,+MD4131", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4136", 0.002, "Ashburn,+VA10163", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4136", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4136", 0.001, "Relay,+MD5801", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4136", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4136", 0.001, "Relay,+MD4127", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6397", 0.001, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6397", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6397", 0.001, "New+York,+NY4107", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6397", 0.001, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6397", 0.003, "Relay,+MD4131", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4135", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4135", 0.001, "Pennsauken,+NJ4117", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4135", 0.001, "Pennsauken,+NJ4126", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4135", 0.002, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4135", 0.001, "Pennsauken,+NJ6647", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4135", 0.012, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4135", 0.002, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4135", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4138", 0.002, "Ashburn,+VA10161", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4138", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4138", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4138", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6750", 0.001, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6750", 0.001, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6563", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6563", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6563", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6563", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.002, "Richardson,+TX5500", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.013, "San+Jose,+CA4119", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.007, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6580", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6726", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6566", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6494", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.008, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6658", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6749", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6587", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.008, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.008, "Chicago,+IL6593", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.008, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6604", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6622", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6444", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6641", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.011, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.011, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6645", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6483", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6573", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6663", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6558", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6737", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6683", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6598", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX1742", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6689", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.005, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.008, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.011, "Anaheim,+CA6502", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6504", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX3549", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.011, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6612", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6419", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.008, "Chicago,+IL6441", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6471", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4080", 0.001, "Dallas,+TX6706", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Brussels,+Belgium4075", 0.002, "Amsterdam4030", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Brussels,+Belgium4075", 0.001, "Brussels,+Belgium4033",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6569", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6569", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6569", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6569", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4108", 0.001, "Orlando,+FL4089", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4108", 0.001, "Orlando,+FL4049", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4108", 0.001, "Orlando,+FL6429", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4108", 0.005, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4108", 0.001, "Orlando,+FL4050", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4108", 0.001, "Orlando,+FL6693", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6754", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6754", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6754", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4059", 0.002, "Tacoma,+WA3251", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4059", 0.001, "Seattle,+WA4019", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4059", 0.001, "Seattle,+WA6432", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4059", 0.001, "Seattle,+WA4060", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4059", 0.015, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6736", 0.001, "Washington,+DC6443", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6736", 0.001, "Washington,+DC9643", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6736", 0.001, "Washington,+DC4139", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6736", 0.001, "Washington,+DC4140", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6736", 0.001, "Washington,+DC6456", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6736", 0.001, "Washington,+DC4142", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6736", 0.001, "Washington,+DC6748", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6690", 0.001, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6690", 0.001, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6690", 0.001, "Kansas+City,+MO6472", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6690", 0.005, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6690", 0.001, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6690", 0.001, "Kansas+City,+MO4106", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6719", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6719", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6719", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6719", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.01, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.001, "Kansas+City,+MO6544", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.001, "Kansas+City,+MO6472", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.001, "Kansas+City,+MO6707", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.001, "Kansas+City,+MO6458", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.001, "Kansas+City,+MO6750", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.001, "Kansas+City,+MO6395", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.001, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.002, "Lees+Summit,+MO5503", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.001, "Kansas+City,+MO4106", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.001, "Kansas+City,+MO6422", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.002, "Lees+Summit,+MO5505", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.001, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.001, "Kansas+City,+MO6690", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO4082", 0.005, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6577", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6577", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6577", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6577", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6580", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6726", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6566", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.005, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6494", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.005, "Kansas+City,+MO4106", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.008, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX4133", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6587", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6604", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6444", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6622", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6641", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.011, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6645", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6483", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6573", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6663", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6737", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6683", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6598", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX1742", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6689", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.007, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6504", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.011, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX3549", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6612", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6419", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6471", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX2635", 0.001, "Dallas,+TX6706", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC9643", 0.001, "Washington,+DC6736", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC9643", 0.001, "Washington,+DC6415", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC9643", 0.002, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC9643", 0.002, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC9643", 0.001, "Washington,+DC6393", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC9643", 0.002, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI6550", 0.001, "Pearl+Harbor,+HI4092", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI6550", 0.001, "Pearl+Harbor,+HI4053", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.004, "San+Jose,+CA4062", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6627", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6539", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6490", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6564", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6556", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6744", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.011, "Dallas,+TX4133", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6591", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6584", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.022, "Pearl+Harbor,+HI4092", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.011, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.011, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6502", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6512", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6520", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6702", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6453", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA6438", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA4099", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Copenhagen4038", 0.001, "Copenhagen4077", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Copenhagen4038", 0.004, "Stockholm,+Sweden4066", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Copenhagen4038", 0.033, "Manasquan,+NJ4086", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Copenhagen4038", 0.003, "Hamburg,+Germany4041", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6419", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6419", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6419", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6419", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6419", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4060", 0.014, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4060", 0.002, "Tacoma,+WA3251", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4060", 0.001, "Seattle,+WA4059", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4060", 0.001, "Seattle,+WA4019", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA4060", 0.001, "Seattle,+WA6432", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4049", 0.001, "Orlando,+FL6429", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4049", 0.001, "Orlando,+FL4089", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4049", 0.009, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4049", 0.001, "Orlando,+FL4108", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4049", 0.005, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4049", 0.001, "Orlando,+FL6459", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4049", 0.001, "Orlando,+FL6693", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4049", 0.005, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4017", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4017", 0.029, "London4045", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4017", 0.001, "New+York,+NY4134", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6559", 0.001, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6559", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6559", 0.001, "New+York,+NY4107", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6559", 0.001, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6696", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6696", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6696", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6407", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6452", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.021, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6463", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6609", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.002, "San+Jose,+CA4095", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6619", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6563", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.016, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6719", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6590", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA3402", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6583", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6479", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6577", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6569", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA6758", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.002, "Rancho+Cordova,+CA5507", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4113", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6748", 0.001, "Washington,+DC6736", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6748", 0.001, "Washington,+DC6543", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6748", 0.001, "Washington,+DC6415", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6748", 0.002, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6748", 0.002, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6748", 0.001, "Washington,+DC6393", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6748", 0.002, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Frankfurt4040", 0.004, "Milan,+Italy4085", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Frankfurt4040", 0.001, "Frankfurt4079", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Frankfurt4040", 0.004, "Hamburg,+Germany4081", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6583", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6583", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6583", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6583", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4020", 0.003, "Pennsauken,+NJ4125", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4020", 0.003, "Pennsauken,+NJ4126", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4020", 0.001, "Springfield,+MA4023", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4020", 0.002, "Boston5499", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4020", 0.001, "Springfield,+MA4025", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4020", 0.001, "Springfield,+MA6406", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4020", 0.002, "New+York,+NY4137", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4020", 0.008, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6455", 0.001, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6455", 0.001, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Ashburn,+VA10161", 0.002, "Relay,+MD4138", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4139", 0.001, "Washington,+DC6736", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4139", 0.001, "Washington,+DC6543", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4139", 0.001, "Washington,+DC6415", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4139", 0.002, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4139", 0.002, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4139", 0.002, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4139", 0.001, "Washington,+DC6393", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Ashburn,+VA10162", 0.002, "Relay,+MD4131", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4023", 0.001, "Springfield,+MA4020", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4023", 0.008, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4023", 0.002, "Boston5509", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4023", 0.001, "Springfield,+MA4025", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4023", 0.003, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4023", 0.001, "Springfield,+MA6406", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Ashburn,+VA10163", 0.002, "Relay,+MD4136", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Ashburn,+VA10163", 0.002, "Relay,+MD4127", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4025", 0.008, "Chicago,+IL4122", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4025", 0.001, "Springfield,+MA4020", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA4025", 0.001, "Springfield,+MA4023", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6388", 0.001, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6388", 0.001, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4050", 0.001, "Orlando,+FL6429", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4050", 0.001, "Orlando,+FL4089", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4050", 0.009, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4050", 0.001, "Orlando,+FL4108", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4050", 0.005, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4050", 0.001, "Orlando,+FL6459", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4050", 0.005, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4050", 0.001, "Orlando,+FL6693", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockholm,+Sweden4066", 0.001, "Stockholm,+Sweden4097",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Stockholm,+Sweden4066", 0.004, "Copenhagen4038", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6414", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6414", 0.001, "Chicago,+IL4122", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6414", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6414", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6414", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6414", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4022", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4022", 0.029, "London4084", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4022", 0.029, "London4083", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4024", 0.029, "London4084", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4024", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4024", 0.001, "New+York,+NY4134", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Reston,+VA5496", 0.001, "Reston,+VA5497", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Reston,+VA5496", 0.002, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Reston,+VA5497", 0.001, "Reston,+VA5496", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Reston,+VA5497", 0.002, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.002, "Pennsauken,+NJ4135", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6580", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6726", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6645", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.021, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6483", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6573", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6566", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.01, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6494", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6737", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.009, "Orlando,+FL4089", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6587", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6749", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6598", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6689", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX1742", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.007, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6504", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX3549", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6604", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6612", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6419", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6622", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6444", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6641", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6706", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4028", 0.013, "Dallas,+TX6471", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6496", 0.001, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6496", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6496", 0.001, "New+York,+NY6446", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6496", 0.001, "New+York,+NY4107", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6496", 0.001, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6496", 0.001, "New+York,+NY6427", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Boston5509", 0.002, "Springfield,+MA4023", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Boston5509", 0.001, "Boston5499", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "Relay,+MD4093", 0.002, "Washington,+DC6443", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.002, "Reston,+VA5497", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD4138", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.002, "Pennsauken,+NJ4135", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD6696", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.002, "Washington,+DC6456", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.002, "Washington,+DC6469", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD6431", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.002, "Washington,+DC4140", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.002, "Washington,+DC4142", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.002, "Washington,+DC6748", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.006, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.002, "Washington,+DC9643", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD6518", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD6461", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.002, "Washington,+DC4139", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD6551", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD6449", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.021, "San+Jose,+CA4112", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD4131", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD6576", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD6675", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD4118", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD4093", 0.001, "Relay,+MD4127", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ4125", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ4117", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ4126", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ4135", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.002, "New+York,+NY4124", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.021, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ6516", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.003, "Springfield,+MA4023", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ6614", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ6525", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.021, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ6624", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ6448", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ6728", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ6486", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.002, "Relay,+MD4118", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ4091", 0.001, "Pennsauken,+NJ6754", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4140", 0.001, "Washington,+DC6736", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4140", 0.001, "Washington,+DC6543", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4140", 0.001, "Washington,+DC6415", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4140", 0.002, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4140", 0.002, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4140", 0.002, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4140", 0.001, "Washington,+DC6393", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4142", 0.001, "Washington,+DC6736", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4142", 0.001, "Washington,+DC6543", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4142", 0.001, "Washington,+DC6415", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4142", 0.002, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC4142", 0.001, "Washington,+DC6393", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6590", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6590", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6590", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6590", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA3957", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA3957", 0.001, "Atlanta,+GA6565", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA3957", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA3957", 0.007, "Dallas,+TX6641", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6502", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6502", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6502", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6502", 0.011, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6502", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6502", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6502", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tokyo4069", 0.016, "Hong+Kong4042", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Tokyo4069", 0.044, "Stockton,+CA4064", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Tokyo4069", 0.001, "Tokyo4070", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "Dallas,+TX6504", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6504", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6504", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6504", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6504", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6422", 0.001, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6422", 0.001, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6422", 0.001, "Kansas+City,+MO4106", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA6477", 0.001, "San+Jose,+CA4095", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6438", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6438", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6438", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6438", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6438", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6438", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Hong+Kong6421", 0.001, "Hong+Kong4042", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Hong+Kong6421", 0.057, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4107", 0.001, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4107", 0.002, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4107", 0.001, "New+York,+NY6397", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4107", 0.001, "New+York,+NY6559", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4107", 0.001, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4107", 0.001, "New+York,+NY6496", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4107", 0.001, "New+York,+NY4134", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4107", 0.001, "New+York,+NY6427", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4107", 0.001, "New+York,+NY6532", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4107", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4107", 0.001, "New+York,+NY6446", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4107", 0.003, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Paris4090", 0.003, "London4044", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "Paris4090", 0.005, "Milan,+Italy4046", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Paris4090", 0.001, "Paris4051", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "Munich,+Germany4087", 0.005, "Hamburg,+Germany4041", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6439", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA3402", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA3402", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA3402", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA3402", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA3402", 0.004, "Anaheim,+CA6512", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tokyo4070", 0.001, "Tokyo4071", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "Tokyo4070", 0.016, "Hong+Kong4042", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Tokyo4070", 0.04, "Tacoma,+WA3251", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Tokyo4070", 0.028, "Singapore4094", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Tokyo4070", 0.04, "Tacoma,+WA4114", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Tokyo4070", 0.001, "Tokyo4069", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "Cheyenne,+WY6542", 0.001, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6542", 0.001, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tokyo4071", 0.028, "Singapore4094", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Tokyo4071", 0.044, "Stockton,+CA4064", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Tokyo4071", 0.001, "Tokyo4070", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "Stockton,+CA4064", 0.021, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA6602", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA6407", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.002, "San+Jose,+CA4119", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.021, "Pearl+Harbor,+HI4053", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA6452", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA6463", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA6563", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA6719", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA3402", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA6590", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA6583", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.044, "Tokyo4071", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA6479", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA6577", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA6569", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA6758", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.044, "Tokyo4069", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.062, "Sydney,+Australia4068", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4064", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6407", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.021, "Pearl+Harbor,+HI4053", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6452", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.021, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6463", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6609", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.002, "San+Jose,+CA4095", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6619", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6563", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.016, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6719", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6590", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA3402", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6583", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6479", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6569", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6577", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA6758", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.002, "Rancho+Cordova,+CA5507", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4065", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6512", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6512", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6512", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6512", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6512", 0.004, "Stockton,+CA3402", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6512", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6512", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6444", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6444", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6444", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6444", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6444", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.001, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.002, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.001, "New+York,+NY4107", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.001, "New+York,+NY6397", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.001, "New+York,+NY6559", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.001, "New+York,+NY4124", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.001, "New+York,+NY6496", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.003, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.001, "New+York,+NY6605", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.001, "New+York,+NY6532", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.001, "New+York,+NY6427", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.007, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.001, "New+York,+NY6606", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.001, "New+York,+NY4137", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4116", 0.001, "New+York,+NY6446", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY4024", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.002, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY6751", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY4017", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY6752", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY6559", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY6397", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY4107", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY6496", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY4134", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.007, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY4137", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.008, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.003, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY6532", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY6427", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY6446", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.002, "Manasquan,+NJ4047", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4048", 0.001, "New+York,+NY4022", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6431", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6431", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6431", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Santa+Clara,+CA5508", 0.002, "San+Jose,+CA4119", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6758", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6758", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6758", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6758", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6629", 0.001, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6629", 0.001, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6520", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6520", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6520", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6520", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6520", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6520", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX3549", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX3549", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX3549", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX3549", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX3549", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Manasquan,+NJ4047", 0.002, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Manasquan,+NJ4047", 0.03, "London4084", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Manasquan,+NJ4047", 0.001, "Manasquan,+NJ4086", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Manasquan,+NJ4047", 0.003, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Manasquan,+NJ4047", 0.03, "London4083", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6415", 0.001, "Washington,+DC6443", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6415", 0.001, "Washington,+DC9643", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6415", 0.001, "Washington,+DC6469", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6415", 0.001, "Washington,+DC4139", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6415", 0.001, "Washington,+DC4140", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6415", 0.001, "Washington,+DC6456", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6415", 0.001, "Washington,+DC6748", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6415", 0.001, "Washington,+DC4142", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6441", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6441", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6441", 0.008, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6441", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6441", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6441", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL6693", 0.001, "Orlando,+FL4089", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL6693", 0.001, "Orlando,+FL4049", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL6693", 0.005, "Atlanta,+GA2347", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL6693", 0.001, "Orlando,+FL4050", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL6693", 0.001, "Orlando,+FL4108", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Frankfurt4079", 0.004, "Paris4051", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Frankfurt4079", 0.001, "Frankfurt4040", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6453", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6453", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6453", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6453", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6453", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6453", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockholm,+Sweden4097", 0.004, "Copenhagen4077", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockholm,+Sweden4097", 0.001, "Stockholm,+Sweden4066",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Copenhagen4077", 0.004, "Stockholm,+Sweden4097", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Copenhagen4077", 0.006, "London4044", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Copenhagen4077", 0.001, "Copenhagen4038", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Copenhagen4077", 0.003, "Hamburg,+Germany4081", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "London4044", 0.006, "Copenhagen4077", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "London4044", 0.001, "London4045", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "London4044", 0.003, "Paris4090", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "London4044", 0.003, "Brussels,+Belgium4033", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "London4044", 0.001, "London4083", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "London4044", 0.004, "Dublin,+Ireland4039", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Richardson,+TX5500", 0.002, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Richardson,+TX5500", 0.002, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "London4045", 0.001, "London4044", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "London4045", 0.029, "New+York,+NY4017", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4124", 0.001, "New+York,+NY6606", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4124", 0.001, "New+York,+NY4137", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4124", 0.001, "New+York,+NY4129", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4124", 0.001, "New+York,+NY4088", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4124", 0.001, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4124", 0.007, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4124", 0.001, "New+York,+NY4134", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4124", 0.002, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4124", 0.001, "New+York,+NY6605", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4053", 0.022, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4053", 0.001, "Pearl+Harbor,+HI6400", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4053", 0.001, "Pearl+Harbor,+HI4092", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4053", 0.021, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4053", 0.021, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4053", 0.021, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4053", 0.022, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI4053", 0.001, "Pearl+Harbor,+HI6550", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4129", 0.001, "New+York,+NY4137", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4129", 0.001, "New+York,+NY4124", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4129", 0.001, "New+York,+NY4134", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4129", 0.001, "New+York,+NY6524", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4129", 0.003, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4089", 0.001, "Orlando,+FL4049", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4089", 0.001, "Orlando,+FL6429", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4089", 0.001, "Orlando,+FL4050", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4089", 0.001, "Orlando,+FL4108", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4089", 0.009, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4089", 0.009, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL4089", 0.001, "Orlando,+FL6693", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6518", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6518", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6518", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6516", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6516", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6516", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6517", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6517", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6449", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6449", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6449", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6448", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6448", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6448", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6604", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6604", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6604", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6604", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6604", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6407", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6407", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6407", 0.001, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA6407", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6464", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6464", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6464", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6464", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6464", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6539", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6539", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6539", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6539", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6539", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6539", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6458", 0.001, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6458", 0.001, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6458", 0.005, "Dallas,+TX6598", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6458", 0.001, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6458", 0.001, "Kansas+City,+MO4106", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4134", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4134", 0.001, "New+York,+NY4129", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4134", 0.001, "New+York,+NY4137", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4134", 0.001, "New+York,+NY4024", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4134", 0.001, "New+York,+NY6751", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4134", 0.001, "New+York,+NY4017", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4134", 0.001, "New+York,+NY6752", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4134", 0.001, "New+York,+NY4107", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4134", 0.001, "New+York,+NY4124", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6530", 0.011, "Cheyenne,+WY6413", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4137", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4137", 0.001, "New+York,+NY4129", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4137", 0.002, "Springfield,+MA4020", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4137", 0.001, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4137", 0.001, "New+York,+NY4124", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4137", 0.001, "New+York,+NY4134", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA6555", 0.001, "Tacoma,+WA6408", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA6555", 0.001, "Tacoma,+WA3251", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA6555", 0.001, "Tacoma,+WA4114", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA6432", 0.001, "Seattle,+WA4059", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA6432", 0.001, "Seattle,+WA4060", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6465", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6465", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6465", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6525", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6525", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6525", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.001, "Cheyenne,+WY6455", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.001, "Cheyenne,+WY2439", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.001, "Cheyenne,+WY6723", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.001, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.009, "Tacoma,+WA3251", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.002, "Denver,+Colorado5511", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.009, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.001, "Cheyenne,+WY6629", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.001, "Cheyenne,+WY6388", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.009, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.001, "Cheyenne,+WY6413", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.009, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY4034", 0.001, "Cheyenne,+WY6542", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6610", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4112", 0.001, "San+Jose,+CA4062", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4112", 0.001, "San+Jose,+CA4095", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4112", 0.004, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4112", 0.002, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4112", 0.001, "San+Jose,+CA4132", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4112", 0.021, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4112", 0.004, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6612", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6612", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6612", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6612", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6612", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.021, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6602", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6407", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.057, "Hong+Kong6421", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.021, "Pearl+Harbor,+HI4053", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6452", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6609", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6463", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6619", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.021, "Pearl+Harbor,+HI4092", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6563", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA4113", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6719", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA3402", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6590", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6583", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6479", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6569", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6577", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA6758", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.001, "Stockton,+CA4065", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.062, "Sydney,+Australia4068", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.002, "Rancho+Cordova,+CA5514", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.002, "San+Jose,+CA4112", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Stockton,+CA4096", 0.002, "San+Jose,+CA4132", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6603", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6603", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6603", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6603", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6603", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6531", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6531", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6531", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6531", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6531", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6471", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6471", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6471", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6471", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6471", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6751", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6751", 0.001, "New+York,+NY4134", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4119", 0.001, "San+Jose,+CA4062", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4119", 0.001, "San+Jose,+CA4095", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4119", 0.013, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4119", 0.002, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4119", 0.002, "Santa+Clara,+CA5508", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4119", 0.001, "San+Jose,+CA4132", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4119", 0.004, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6752", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY6752", 0.001, "New+York,+NY4134", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY2439", 0.001, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY2439", 0.001, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA6701", 0.016, "Roachdale,+IN6552", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Tacoma,+WA6701", 0.001, "Tacoma,+WA3251", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Singapore4094", 0.028, "Tokyo4071", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Singapore4094", 0.028, "Tokyo4070", delay_add, delay_multiply,
          &i, &out);
  AddLink(bw, "Kansas+City,+MO6395", 0.001, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6395", 0.001, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6395", 0.001, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6395", 0.001, "Kansas+City,+MO4106", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6468", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6468", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6468", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6468", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6468", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6540", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6540", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6540", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6552", 0.001, "Roachdale,+IN4111", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6552", 0.016, "Tacoma,+WA6701", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6552", 0.001, "Roachdale,+IN4056", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD5801", 0.001, "Relay,+MD4136", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD5801", 0.011, "Dallas,+TX6573", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD5801", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL6429", 0.001, "Orlando,+FL4089", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL6429", 0.001, "Orlando,+FL4049", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL6429", 0.001, "Orlando,+FL4050", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Orlando,+FL6429", 0.001, "Orlando,+FL4108", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6723", 0.001, "Cheyenne,+WY4034", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Cheyenne,+WY6723", 0.001, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6461", 0.001, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6461", 0.001, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Relay,+MD6461", 0.001, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Sydney,+Australia4067", 0.064, "Tacoma,+WA3251", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Sydney,+Australia4067", 0.001, "Sydney,+Australia6437",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Sydney,+Australia4068", 0.001, "Sydney,+Australia6437",
          delay_add, delay_multiply, &i, &out);
  AddLink(bw, "Sydney,+Australia4068", 0.062, "Stockton,+CA4064", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Sydney,+Australia4068", 0.062, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA2338", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA2338", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA2338", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Amsterdam4030", 0.001, "Amsterdam4072", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Amsterdam4030", 0.002, "Brussels,+Belgium4075", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6622", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6622", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6622", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6622", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6622", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6611", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6611", 0.001, "Chicago,+IL4122", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6611", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6611", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6611", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6611", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6443", 0.001, "Washington,+DC6736", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6443", 0.001, "Washington,+DC6543", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6443", 0.001, "Washington,+DC6415", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6443", 0.002, "Relay,+MD4093", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6443", 0.002, "Relay,+MD4110", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6443", 0.002, "Relay,+MD4054", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Washington,+DC6443", 0.001, "Washington,+DC6393", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6399", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6544", 0.001, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6544", 0.001, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6544", 0.006, "Cheyenne,+WY6746", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6544", 0.001, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6544", 0.001, "Kansas+City,+MO4106", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6627", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6627", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6627", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6627", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6627", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6627", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.002, "Richardson,+TX5500", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.009, "Orlando,+FL4049", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6580", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6726", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6566", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6494", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6658", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6749", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6587", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.009, "Orlando,+FL4050", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6604", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6622", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6444", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6641", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.012, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6645", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6483", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6573", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6558", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6737", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.005, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.005, "Kansas+City,+MO6690", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.011, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6598", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX1742", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6689", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6504", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX3549", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6612", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6419", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.007, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6471", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX4015", 0.001, "Dallas,+TX6706", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6472", 0.001, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6472", 0.001, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6472", 0.001, "Kansas+City,+MO6690", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6472", 0.001, "Kansas+City,+MO4105", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Kansas+City,+MO6472", 0.001, "Kansas+City,+MO4106", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6483", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6483", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6483", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6483", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6483", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6556", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6556", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6556", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6556", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6556", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6556", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6558", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6558", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6558", 0.001, "Dallas,+TX6573", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA6406", 0.001, "Springfield,+MA4020", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Springfield,+MA6406", 0.001, "Springfield,+MA4023", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI6400", 0.001, "Pearl+Harbor,+HI4092", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pearl+Harbor,+HI6400", 0.001, "Pearl+Harbor,+HI4053", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6636", 0.001, "Roachdale,+IN4111", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Roachdale,+IN6636", 0.001, "Roachdale,+IN4056", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6481", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6481", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6481", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6628", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6628", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA6628", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Seattle,+WA6450", 0.001, "Seattle,+WA4019", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.001, "New+York,+NY4107", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.001, "New+York,+NY6397", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.001, "New+York,+NY6559", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.002, "Manasquan,+NJ4086", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.001, "New+York,+NY4116", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.001, "New+York,+NY4124", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.007, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.001, "New+York,+NY6496", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.001, "New+York,+NY6532", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.001, "New+York,+NY6524", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.001, "New+York,+NY6427", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.001, "New+York,+NY4048", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.001, "New+York,+NY6446", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "New+York,+NY4088", 0.008, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.007, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6724", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.008, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6468", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6492", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL4122", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6639", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6747", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6593", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6586", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6595", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6669", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6414", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6611", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6603", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6441", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6531", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL1391", 0.001, "Chicago,+IL6621", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Denver,+Colorado5501", 0.002, "Cheyenne,+WY4076", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6614", 0.001, "Pennsauken,+NJ4109", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6614", 0.001, "Pennsauken,+NJ4052", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Pennsauken,+NJ6614", 0.001, "Pennsauken,+NJ4091", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Boston5499", 0.002, "Springfield,+MA4020", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Boston5499", 0.001, "Boston5509", delay_add, delay_multiply, &i,
          &out);
  AddLink(bw, "Atlanta,+GA2344", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA2344", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA2344", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dublin,+Ireland4039", 0.004, "London4044", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Lees+Summit,+MO5503", 0.002, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Lees+Summit,+MO5504", 0.002, "Kansas+City,+MO4043", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA6742", 0.001, "San+Jose,+CA4062", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA6742", 0.001, "San+Jose,+CA4132", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA2347", 0.001, "Atlanta,+GA4032", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA2347", 0.001, "Atlanta,+GA4074", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA2347", 0.001, "Atlanta,+GA4102", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Atlanta,+GA2347", 0.005, "Orlando,+FL6693", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Lees+Summit,+MO5505", 0.002, "Kansas+City,+MO4082", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6702", 0.001, "Anaheim,+CA4099", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6702", 0.001, "Anaheim,+CA4101", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6702", 0.001, "Anaheim,+CA4031", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6702", 0.001, "Anaheim,+CA4073", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6702", 0.001, "Anaheim,+CA6721", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Anaheim,+CA6702", 0.001, "Anaheim,+CA4100", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4132", 0.001, "San+Jose,+CA4062", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4132", 0.001, "San+Jose,+CA4119", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4132", 0.001, "San+Jose,+CA4112", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4132", 0.001, "San+Jose,+CA6742", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "San+Jose,+CA4132", 0.002, "Stockton,+CA4096", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6706", 0.001, "Dallas,+TX2635", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6706", 0.001, "Dallas,+TX4015", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6706", 0.001, "Dallas,+TX4080", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6706", 0.013, "New+York,+NY4028", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Dallas,+TX6706", 0.001, "Dallas,+TX4115", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6621", 0.001, "Chicago,+IL1391", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6621", 0.001, "Chicago,+IL4122", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6621", 0.001, "Chicago,+IL4036", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6621", 0.001, "Chicago,+IL1484", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6621", 0.001, "Chicago,+IL4037", delay_add,
          delay_multiply, &i, &out);
  AddLink(bw, "Chicago,+IL6621", 0.001, "Chicago,+IL4104", delay_add,
          delay_multiply, &i, &out);
  return out;
}

}  // namespace net
}  // namespace ncode
