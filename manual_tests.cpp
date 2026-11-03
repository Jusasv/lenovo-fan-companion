#include "manual_rules.h"
#include <cstdlib>
#include <iostream>
#include <vector>
int main() {
    int failures = 0;
    int checks = 0;
    auto check = [&](bool value) { ++checks; if (!value) ++failures; };
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
    ManualRecovery recovery;
    check(recovery.can_boost(true, false));
    check(!recovery.can_boost(false, false) && !recovery.can_boost(true, true));
    recovery.requested(true);
    check(recovery.normalRequired && !recovery.can_boost(true, false));
    recovery.completed(true, {0, 31});
    check(recovery.normalRequired && !recovery.can_boost(true, false));
    recovery.completed(false, {5, 0});
    check(recovery.normalRequired);
    recovery.completed(false, {0, 0});
    check(!recovery.normalRequired && recovery.can_boost(true, false));
    recovery.requested(true);
    recovery.completed(true, {5, 0});
    check(!recovery.normalRequired); // Failed Boost still recovered via accepted Normal.
    SessionHistory history;
    check(history.entries().empty());
    for (int index = 0; index < 55; ++index) history.add(std::to_wstring(index));
    check(history.entries().size() == 50 && history.entries().front() == L"5" && history.entries().back() == L"54");
    history.add(std::wstring(900, L'x'));
    check(history.entries().size() == 50 && history.entries().back().size() == 600);
    if (failures) std::cout << "Manual sequence tests failed\n";
    else std::cout << checks << " manual sequence/recovery/history checks passed\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
