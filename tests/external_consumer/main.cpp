#include <3dgs/OffscreenRenderer.h>

#include <utility>

void linkPublicApi() {
    vkgs::OffscreenConfig configuration;
    vkgs::OffscreenRenderer renderer(std::move(configuration));
}

int main() {
    auto* volatile linkedSymbol = &linkPublicApi;
    return linkedSymbol == nullptr ? 1 : 0;
}
