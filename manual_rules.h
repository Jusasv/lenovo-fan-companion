#pragma once
#include <string_view>
#include <string>
#include <deque>
#include <utility>
namespace companion {
inline bool manual_target(std::wstring_view model, std::wstring_view bios) {
    return model == L"80NV" && bios == L"CDCN54WW";
}
struct ManualOutcome { unsigned requestedError{}, restoreError{}; };
struct ManualRecovery {
    bool normalRequired = false;
    bool can_boost(bool eligible, bool active) const { return eligible && !active && !normalRequired; }
    void requested(bool boost) { if (boost) normalRequired = true; }
    void completed(bool boost, const ManualOutcome& result) {
        normalRequired = (boost ? result.restoreError : result.requestedError) != 0;
    }
};
class SessionHistory {
    std::deque<std::wstring> records;
public:
    void add(std::wstring entry) {
        if (entry.size() > 600) entry.resize(600);
        if (records.size() == 50) records.pop_front();
        records.push_back(std::move(entry));
    }
    const std::deque<std::wstring>& entries() const { return records; }
};
template<class Send, class Wait>
ManualOutcome manual_sequence(bool boost, Send send, Wait wait) {
    ManualOutcome result;
    result.requestedError = send(boost ? 1u : 0u);
    if (boost) {
        if (result.requestedError == 0) wait();
        // Restore even if Boost failed: an error does not establish firmware state.
        result.restoreError = send(0u);
    }
    return result;
}
}
