#pragma once

#include <cstdint>

// Public plugin interface. Include this header in the calling plugin and obtain
namespace PNGII::API
{
	inline constexpr std::uint32_t kVersion1 = 1;
	inline constexpr char kExportName[] = "PNGII_RequestAPI";

	enum class Result : std::uint32_t
	{
		kSuccess = 0,
		kNoMatch = 1,
		kNotReady = 2,
		kBufferTooSmall = 3,
		kInvalidArgument = 4,
		kError = 5
	};

	// Call on Skyrim's UI thread after a save or new game has loaded.
	using ResolveIconFunction = Result (*)(std::uint32_t formID, const char* iconLabel,
		std::int32_t soulStatus, char* path, std::uint32_t capacity,
		std::uint32_t* requiredSize) noexcept;

	struct InterfaceV1
	{
		std::uint32_t version;
		std::uint32_t structSize;
		ResolveIconFunction resolveIcon;
	};

	// The returned table remains valid while PNGIconInjector.dll is loaded.
	using RequestAPIFunction = const void* (*)(std::uint32_t version) noexcept;
}
