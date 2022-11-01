#include "DXGIExtensions.h"

bool operator==(const DXGI_SAMPLE_DESC& first, const DXGI_SAMPLE_DESC& second)
{
    return first.Count == second.Count &&
        first.Quality == second.Quality;
}

bool operator!=(const DXGI_SAMPLE_DESC& first, const DXGI_SAMPLE_DESC& second)
{
    return !(first == second);
}

size_t std::hash<DXGI_SAMPLE_DESC>::operator()(const DXGI_SAMPLE_DESC& desc) const
{
    using std::size_t;
    using std::hash;

    return hash<UINT>()(desc.Count)
        ^ hash<UINT>()(desc.Quality);
}
