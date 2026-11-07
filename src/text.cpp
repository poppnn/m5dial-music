#include <assert.h>
#include "text.h"

std::string cleanText(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size();) {
    uint8_t b = s[i];
    size_t n = b < 0x80 ? 1 : b < 0xE0 ? 2 : b < 0xF0 ? 3 : 4;
    if (i + n > s.size()) break;  // séquence coupée (texte tronqué par l'iPhone)
    std::string ch = s.substr(i, n);
    bool drop = n == 4 || ch == "\xEF\xB8\x8F" || ch == "\xEF\xB8\x8E" || ch == "\xE2\x80\x8D" || ch == "\xE2\x83\xA3";
    if (!drop && !(ch == " " && (out.empty() || out.back() == ' '))) out += ch;
    i += n;
  }
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}

// Auto-test, appelé depuis setup().
void cleanTextSelfTest() {
  assert(cleanText("Salut \xF0\x9F\x98\x80 toi") == "Salut toi");                // 😀
  assert(cleanText("\xE2\x9D\xA4\xEF\xB8\x8F Love") == "\xE2\x9D\xA4 Love");      // ❤️ : le cœur U+2764 reste
  assert(cleanText("Caf\xC3\xA9 ") == "Caf\xC3\xA9");
}
