#include "d3d12.h"
#include <DX12Library/RenderTarget.h>

class RenderTargetFormats
{
public:
	static constexpr UINT MAX_RENDER_TARGETS = 8;

	using FormatsArray = DXGI_FORMAT[MAX_RENDER_TARGETS];
	using ConstFormatsArray = const DXGI_FORMAT[MAX_RENDER_TARGETS];

	RenderTargetFormats()
		: m_Count(0)
		, m_Formats()
	{
		for (auto& format : m_Formats)
		{
			format = DXGI_FORMAT_UNKNOWN;
		}
	}

	explicit RenderTargetFormats(const RenderTarget& renderTarget)
		: m_Count(0)
		, m_Formats()
	{
		for (UINT i = 0; i < MAX_RENDER_TARGETS; ++i)
		{
			const auto& texture = renderTarget.GetTexture((AttachmentPoint)i);
			if (texture.IsValid())
			{
				m_Formats[i] = texture.GetD3D12ResourceDesc().Format;
				++m_Count;
			}
			else
			{
				m_Formats[i] = DXGI_FORMAT_UNKNOWN;
			}
		}
	}

	inline UINT GetCount() const { return m_Count; }
	inline const ConstFormatsArray& GetFormats() const { return m_Formats; }

	bool operator==(const RenderTargetFormats& other) const
	{
		if (GetCount() != other.GetCount())
		{
			return false;
		}

		for (UINT i = 0; i < MAX_RENDER_TARGETS; ++i)
		{
			if (GetFormats()[i] != other.GetFormats()[i])
			{
				return false;
			}
		}

		return true;
	}

private:

	UINT m_Count;
	FormatsArray m_Formats;
};

namespace std
{
	template <typename T, int N>
	static std::size_t hasharray(const T(&arr)[N])
	{
		size_t seed = 0;
		for (const T* it = arr; it != (arr + N); ++it)
			seed ^= *it;
		return seed;
	}

	template <>
	struct hash<RenderTargetFormats>
	{
		std::size_t operator()(const RenderTargetFormats& formats) const
		{
			using std::size_t;
			using std::hash;
			using std::string;

			return (hash<UINT>()(formats.GetCount())
				^ (hasharray(formats.GetFormats()) << 1));
		}
	};
}