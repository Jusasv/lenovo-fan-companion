#pragma once
#include <string_view>
namespace companion {
inline bool manual_target(std::wstring_view model, std::wstring_view bios) {
    return model == L"80NV" && bios == L"CDCN54WW";
}
struct ManualOutcome { unsigned requestedError{}, restoreError{}; };
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
