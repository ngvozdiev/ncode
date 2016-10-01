namespace ncode {
namespace htsim {

template <typename T>
static void PrintTimeDiff(std::ostream& out, T chrono_diff) {
  namespace sc = std::chrono;
  auto diff = sc::duration_cast<sc::milliseconds>(chrono_diff).count();
  auto const msecs = diff % 1000;
  diff /= 1000;
  auto const secs = diff % 60;
  diff /= 60;
  auto const mins = diff % 60;
  diff /= 60;
  auto const hours = diff % 24;
  diff /= 24;
  auto const days = diff;

  bool printed_earlier = false;
  if (days >= 1) {
    printed_earlier = true;
    out << days << (1 != days ? " days" : " day") << ' ';
  }
  if (printed_earlier || hours >= 1) {
    printed_earlier = true;
    out << hours << (1 != hours ? " hours" : " hour") << ' ';
  }
  if (printed_earlier || mins >= 1) {
    printed_earlier = true;
    out << mins << (1 != mins ? " minutes" : " minute") << ' ';
  }
  if (printed_earlier || secs >= 1) {
    printed_earlier = true;
    out << secs << (1 != secs ? " seconds" : " second") << ' ';
  }
  if (printed_earlier || msecs >= 1) {
    printed_earlier = true;
    out << msecs << (1 != msecs ? " milliseconds" : " millisecond");
  }
}

static std::chrono::milliseconds TimeNow() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::high_resolution_clock::now().time_since_epoch());
}

//ProgressIndicator::ProgressIndicator(Component* parent,
//                                     const std::string& component_id,
//                                     EventQueueTime end_time)
//    : NoEventComponent(parent, component_id),
//      end_time_(end_time),
//      init_real_time_(0) {
//  kProgressMetric->GetHandle([this] { return Update(); });
//}
//
//double ProgressIndicator::Update() {
//  if (init_real_time_.count() == 0) {
//    init_real_time_ = TimeNow();
//  }
//
//  double sim_time_now = event_queue_.CurrentEventQueueTime().Raw() / 1000.0;
//  double end_time = end_time_.Raw() / 1000.0;
//  double progress = sim_time_now / end_time;
//  assert(progress < 1.0 && "End time in progress indicator wrong");
//
//  std::cout << "\rProgress: " << std::setprecision(3) << (progress * 100.0)
//            << "% ";
//
//  auto real_time_delta = TimeNow() - init_real_time_;
//  if (real_time_delta.count() > 0) {
//    std::cout << "time remaining: ";
//    auto remaining = std::chrono::milliseconds(static_cast<uint64_t>(
//        real_time_delta.count() / progress * (1 - progress)));
//
//    PrintTimeDiff(std::cout, remaining);
//    std::cout << "                ";
//  }
//
//  std::cout << std::flush;
//  return progress;
//}
}
}
