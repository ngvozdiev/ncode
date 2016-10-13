#include <stdio.h>
#include "logging.h"
#include "../../external/backward-cpp/backward.hpp"

namespace ncode {
namespace internal {

void DefaultLogHandler(LogLevel level, const char* filename, int line,
                       const std::string& message) {
  static const char* level_names[] = {"INFO", "WARNING", "ERROR", "FATAL"};

  // We use fprintf() instead of cerr because we want this to work at static
  // initialization time.
  fprintf(stderr, "[%s %s:%d] %s\n", level_names[level], filename, line,
          message.c_str());
  fflush(stderr);  // Needed on MSVC.
}

void NullLogHandler(LogLevel /* level */, const char* /* filename */,
                    int /* line */, const std::string& /* message */) {
  // Nothing.
}

static LogHandler* log_handler_ = &DefaultLogHandler;

LogMessage::LogMessage(LogLevel level, const char* filename, int line)
    : level_(level), filename_(filename), line_(line) {}

LogMessage::~LogMessage() {}

void LogMessage::Finish() {
  log_handler_(level_, filename_, line_, message_);
  if (level_ == LOGLEVEL_FATAL) {
    backward::StackTrace st;
    st.load_here(32);

    backward::Printer printer;
    printer.print(st);

    abort();
  }
}

void LogFinisher::operator=(LogMessage& other) { other.Finish(); }

}  // namespace internal

LogHandler* SetLogHandler(LogHandler* new_func) {
  LogHandler* old = internal::log_handler_;
  if (old == &internal::NullLogHandler) {
    old = NULL;
  }
  if (new_func == NULL) {
    internal::log_handler_ = &internal::NullLogHandler;
  } else {
    internal::log_handler_ = new_func;
  }
  return old;
}

}  // namespace ncode
