#include "manual_rules.h"
#include <cstdlib>
#include <iostream>
#include <vector>
int main() {
    int failures = 0;
    auto check = [&](bool value) { if (!value) ++failures; };
    using namespace companion;
    check(manual_target(L"80NV", L"CDCN54WW"));
    check(!manual_target(L"80NV", L"UNKNOWN"));
    check(!manual_target(L"OTHER", L"CDCN54WW"));
    std::vector<unsigned> calls;
    bool waited = false;
    auto send = [&](unsigned mode) { calls.push_back(mode); return 0u; };
    auto wait = [&] { waited = true; };
    auto result = manual_sequence(false, send, wait);
    check(calls == std::vector<unsigned>{0} && !waited && result.requestedError == 0);
    calls.clear();
    result = manual_sequence(true, send, wait);
    check(calls == std::vector<unsigned>({1, 0}) && waited && result.restoreError == 0);
    calls.clear(); waited = false;
    result = manual_sequence(true, [&](unsigned mode) { calls.push_back(mode); return mode ? 5u : 0u; }, wait);
    check(calls == std::vector<unsigned>({1, 0}) && !waited && result.requestedError == 5);
    result = manual_sequence(true, [](unsigned mode) { return mode ? 0u : 5u; }, wait);
    check(result.restoreError == 5);
    result = manual_sequence(false, [](unsigned) { return 32u; }, wait);
    check(result.requestedError == 32);
    std::cout << (failures ? "Manual sequence tests failed\n" : "8 manual sequence checks passed\n");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
