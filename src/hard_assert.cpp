module gbemu;

namespace gbemu {

void
hardAssert(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << "gbemu: internal error: " << message << '\n';
    std::abort();
  }
}

}
