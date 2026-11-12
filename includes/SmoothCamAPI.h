#pragma once
#include <functional>
#include <stdint.h>

#if !defined(SMOOTHCAM_API_SKSE) && !defined(SMOOTHCAM_API_COMMONLIB)
static_assert(0, "SmoothCamAPI: SKSE API type not defined. Define one of (SMOOTHCAM_API_SKSE, SMOOTHCAM_API_COMMONLIB)")
#endif

#if defined(SMOOTHCAM_API_SKSE) && defined(SMOOTHCAM_API_COMMONLIB)
static_assert(0, "SmoothCamAPI: Define one of (SMOOTHCAM_API_SKSE, SMOOTHCAM_API_COMMONLIB), not both.")
#endif

#ifdef SMOOTHCAM_API_SKSE
#define SMOOTHCAM_API_LOGGER _MESSAGE
#elif defined(SMOOTHCAM_API_COMMONLIB)
#define SMOOTHCAM_API_LOGGER SKSE::log::info
#endif

/*
* For modders: Copy this file into your own project if you wish to use this API
*
* #define SMOOTHCAM_API_SKSE if using SKSE
* #define SMOOTHCAM_API_COMMONLIB if using CommonLibSSE
*/
namespace SmoothCamAPI {
	constexpr const auto SmoothCamPluginName = "SmoothCam";

#ifdef SMOOTHCAM_API_COMMONLIB
	using PluginHandle = SKSE::PluginHandle;
	using Actor = RE::Actor;
	using TESObjectREFR = RE::TESObjectREFR;
	using NiCamera = RE::NiCamera;
	using NiPoint3 = RE::NiPoint3;
#endif

	enum class InterfaceVersion : uint8_t {
		V1,
		V2,
		V3
	};

	enum class APIResult : uint8_t {
		OK,

		NotOwner,

		MustKeep,

		AlreadyGiven,

		AlreadyTaken,

		BadThread,
	};

	class IVSmoothCam1 {
		public:
		[[nodiscard]] virtual unsigned long GetSmoothCamThreadId() const noexcept = 0;

		[[nodiscard]] virtual APIResult RequestCameraControl(PluginHandle myPluginHandle) noexcept = 0;

		[[nodiscard]] virtual APIResult RequestCrosshairControl(PluginHandle myPluginHandle,
			bool restoreDefaults = true) noexcept = 0;

		[[nodiscard]] virtual APIResult RequestStealthMeterControl(PluginHandle myPluginHandle,
			bool restoreDefaults = true) noexcept = 0;

		virtual PluginHandle GetCameraOwner() const noexcept = 0;

		virtual PluginHandle GetCrosshairOwner() const noexcept = 0;

		virtual PluginHandle GetStealthMeterOwner() const noexcept = 0;

		virtual APIResult ReleaseCameraControl(PluginHandle myPluginHandle) noexcept = 0;

		virtual APIResult ReleaseCrosshairControl(PluginHandle myPluginHandle) noexcept = 0;

		virtual APIResult ReleaseStealthMeterControl(PluginHandle myPluginHandle) noexcept = 0;
	};

	class IVSmoothCam2 : public IVSmoothCam1 {
		public:
		virtual NiPoint3 GetLastCameraPosition() const noexcept = 0;

		virtual APIResult RequestInterpolatorUpdates(PluginHandle myPluginHandle, bool allowUpdates) noexcept = 0;

		virtual APIResult SendToGoalPosition(PluginHandle myPluginHandle, bool shouldMoveToGoal, bool moveNow = false,
			const Actor* ref = nullptr) noexcept = 0;

		virtual void GetGoalPosition(TESObjectREFR* ref, NiPoint3& world, NiPoint3& local) const noexcept = 0;

		virtual bool IsCameraEnabled() const noexcept = 0;
	};

	class IVSmoothCam3 : public IVSmoothCam2 {
		public:
		virtual void EnableUnlockedHorseAim(bool enable) noexcept = 0;
	};

	struct PluginCommand {
		enum class Type : uint8_t {
			RequestInterface,
		};

		uint32_t header = 0x9007CA50;
		Type type;
		void* commandStructure = nullptr;
	};

	struct InterfaceRequest {
		InterfaceVersion interfaceVersion;
	};

	struct PluginResponse {
		enum class Type : uint8_t {
			Error,
			InterfaceProvider,
		};

		Type type;
		void* responseData = nullptr;
	};

	struct InterfaceContainer {
		void* interfaceInstance = nullptr;
		InterfaceVersion interfaceVersion;
	};

	using InterfaceLoaderCallback = std::function<void(
		void* interfaceInstance, InterfaceVersion interfaceVersion
	)>;

#ifdef SMOOTHCAM_API_SKSE
	[[nodiscard]]
	inline bool RequestInterface(SKSEMessagingInterface* skseMessaging, PluginHandle myPluginHandle,
		InterfaceVersion version = InterfaceVersion::V3) noexcept
	{
		InterfaceRequest req = {};
		req.interfaceVersion = version;

		PluginCommand cmd = {};
		cmd.type = PluginCommand::Type::RequestInterface;
		cmd.commandStructure = &req;

		return skseMessaging->Dispatch(
			myPluginHandle,
			0,
			&cmd, sizeof(PluginCommand),
			SmoothCamPluginName
		);
	};

	[[nodiscard]]
	inline bool RegisterInterfaceLoaderCallback(SKSEMessagingInterface* skseMessaging,
		PluginHandle myPluginHandle, InterfaceLoaderCallback&& callback) noexcept
	{
		static InterfaceLoaderCallback storedCallback = callback;

		return skseMessaging->RegisterListener(
			myPluginHandle, SmoothCamPluginName,
			[](SKSEMessagingInterface::Message* msg) {
				if (msg->sender && strcmp(msg->sender, SmoothCamPluginName) != 0) return;
				if (msg->type != 0) return;
				if (msg->dataLen != sizeof(PluginResponse)) return;

				const auto resp = reinterpret_cast<PluginResponse*>(msg->data);
				switch (resp->type) {
					case PluginResponse::Type::InterfaceProvider: {
						auto interfaceContainer = reinterpret_cast<InterfaceContainer*>(resp->responseData);
						storedCallback(
							interfaceContainer->interfaceInstance,
							interfaceContainer->interfaceVersion
						);
						break;
					}
					case PluginResponse::Type::Error: {
						SMOOTHCAM_API_LOGGER("SmoothCam API: Error obtaining interface");
						break;
					}
					default: return;
				}
			}
		);
	}
#elif defined(SMOOTHCAM_API_COMMONLIB)
	[[nodiscard]]
	inline bool RequestInterface(const SKSE::MessagingInterface* skseMessaging,
		InterfaceVersion version = InterfaceVersion::V3) noexcept
	{
		InterfaceRequest req = {};
		req.interfaceVersion = version;

		PluginCommand cmd = {};
		cmd.type = PluginCommand::Type::RequestInterface;
		cmd.commandStructure = &req;

		return skseMessaging->Dispatch(
			0,
			&cmd, sizeof(PluginCommand),
			SmoothCamPluginName
		);
	}

	[[nodiscard]]
	inline bool RegisterInterfaceLoaderCallback(const SKSE::MessagingInterface* skseMessaging,
		InterfaceLoaderCallback&& callback) noexcept
	{
		static InterfaceLoaderCallback storedCallback = callback;

		return skseMessaging->RegisterListener(
			SmoothCamPluginName,
			[](SKSE::MessagingInterface::Message* msg) {
				if (msg->sender && strcmp(msg->sender, SmoothCamPluginName) != 0) return;
				if (msg->type != 0) return;
				if (msg->dataLen != sizeof(PluginResponse)) return;

				const auto resp = reinterpret_cast<PluginResponse*>(msg->data);
				switch (resp->type) {
					case PluginResponse::Type::InterfaceProvider: {
						auto interfaceContainer = reinterpret_cast<InterfaceContainer*>(resp->responseData);
						storedCallback(
							interfaceContainer->interfaceInstance,
							interfaceContainer->interfaceVersion
						);
						break;
					}
					case PluginResponse::Type::Error: {
						SMOOTHCAM_API_LOGGER("SmoothCam API: Error obtaining interface");
						break;
					}
					default: return;
				}
			}
		);
	}
#endif
}
