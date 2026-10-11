#include "check.h"

#include "util/string_table.h"

#include <cstdio>

using crisp::util::Language;
using crisp::util::StringTable;

int main() {
    const auto keys = StringTable::Keys();
    CHECK(!keys.empty());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) {
            CHECK(keys[i - 1] < keys[i]);  // sorted and unique, as the lookup requires
        }
        CHECK(!StringTable::Lookup(keys[i], Language::English).empty());
        CHECK(!StringTable::Lookup(keys[i], Language::Chinese).empty());
    }

    StringTable table;
    CHECK(table.GetLanguage() == Language::English);
    CHECK(table.Get("tab.dashboard") == "Dashboard");
    table.SetLanguage(Language::Chinese);
    CHECK(table.Get("tab.dashboard") == "概览");
    CHECK(table.Get("no.such.key") == "no.such.key");

    std::printf("string_table_test: %u keys checked\n", static_cast<unsigned>(keys.size()));
    return crisp::test::Result();
}
