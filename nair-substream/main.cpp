#define STRICT
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _USE_MATH_DEFINES

#include <Windows.h>
#include <obs-module.h>
#include "../thirdparty/json.hpp"
using json = nlohmann::json;

#include "version.h"

#pragma comment(lib, "obs.lib")

obs_output *output = nullptr;
std::chrono::steady_clock::time_point beginTime = {};
std::string statusMessage = "";
std::string errorMessage = "";

bool isActive = false;
HANDLE threadHandle = nullptr;

//-------------------------------------------
void onStarting(void *x, calldata_t *)
{
	statusMessage = "starting";
	errorMessage = "";
}
void onStarted(void *x, calldata_t *)
{
	statusMessage = "started";
}
void onStopping(void *x, calldata_t *)
{
	statusMessage = "stopping";
}
void onStopped(void *x, calldata_t *param)
{
	statusMessage = "stopped";
	errorMessage = "";

	auto code = calldata_int(param, "code");
	if (code == OBS_OUTPUT_SUCCESS)
		errorMessage = "";
	else if (code == OBS_OUTPUT_BAD_PATH)
		errorMessage = "bad path";
	else if (code == OBS_OUTPUT_CONNECT_FAILED)
		errorMessage = "connect failed";
	else if (code == OBS_OUTPUT_INVALID_STREAM)
		errorMessage = "invalid stream";
	else if (code == OBS_OUTPUT_ERROR)
		errorMessage = "error";
	else if (code == OBS_OUTPUT_DISCONNECTED)
		errorMessage = "disconnected";
	else if (code == OBS_OUTPUT_UNSUPPORTED)
		errorMessage = "unsupported";
	else if (code == OBS_OUTPUT_NO_SPACE)
		errorMessage = "no space";
	else if (code == OBS_OUTPUT_ENCODE_ERROR)
		errorMessage = "encode error";
}

void onReconnect(void *x, calldata_t *)
{
	statusMessage = "reconnect";
}
void onReconnected(void *x, calldata_t *)
{
	statusMessage = "reconnected";
}
void onDeactive(void *x, calldata_t *)
{
	statusMessage = "deactive";
}
//-------------------------------------------

json enumEncoderTypes()
{
	json audio = json::array();
	json video = json::array();

	int i = 0;
	const char *val;
	while (obs_enum_encoder_types(i++, &val))
	{
		json d =
			{{"id", val},
			 {"name", obs_encoder_get_display_name(val)}};

		if (obs_get_encoder_type(val) == OBS_ENCODER_AUDIO)
			audio.push_back(d);
		else
			video.push_back(d);
	}

	json result =
		{{"encoders", {{"audio", audio}, {"video", video}}}};
	return result;
}

void release()
{
	if (!output)
		return;
	obs_output_stop(output);

	auto videoEncoder = obs_output_get_video_encoder(output);
	if (videoEncoder)
	{
		obs_output_set_video_encoder(output, nullptr);
		obs_encoder_release(videoEncoder);
	}

	auto audioEncoder = obs_output_get_audio_encoder(output, 0);
	if (audioEncoder)
	{
		obs_output_set_audio_encoder(output, nullptr, 0);
		obs_encoder_release(audioEncoder);
	}

	auto service = obs_output_get_service(output);
	if (service)
	{
		obs_output_set_service(output, nullptr);
		obs_service_release(service);
	}

	auto signalHandler = obs_output_get_signal_handler(output);
	if (signalHandler)
	{
		signal_handler_disconnect(signalHandler, "starting", &onStarting, nullptr);
		signal_handler_disconnect(signalHandler, "start", &onStarted, nullptr);
		signal_handler_disconnect(signalHandler, "stopping", &onStopping, nullptr);
		signal_handler_disconnect(signalHandler, "stop", &onStopped, nullptr);
		signal_handler_disconnect(signalHandler, "reconnect", &onReconnect, nullptr);
		signal_handler_disconnect(signalHandler, "reconnected_success", &onReconnected, nullptr);
		signal_handler_disconnect(signalHandler, "deactivate", &onDeactive, nullptr);
	}

	obs_output_release(output);
	output = nullptr;
}

json start(json &arg)
{
	blog(LOG_INFO, "start %s", arg.dump().c_str());
	release();

	obs_data *outputParam = obs_data_create_from_json(arg["output"].dump().c_str());
	auto outputInstance = obs_output_create("rtmp_output", "nass-output", outputParam, nullptr);
	blog(LOG_INFO, "output %x ", outputInstance);
	if (!outputInstance)
		return {{"error", "output create failed"}};

	auto serviceParam = obs_data_create_from_json(arg["service"].dump().c_str());
	auto service = obs_service_create("rtmp_custom", "nass-service", serviceParam, nullptr);
	blog(LOG_INFO, "service %x", service);
	if (!service)
		return {{"error", "service create failed"}};
	obs_output_set_service(outputInstance, service);

	auto videoParam = obs_data_create_from_json(arg["video"].dump().c_str());
	auto videoId = arg["videoId"].get_ref<std::string &>().c_str();
	auto videoEncoder = obs_video_encoder_create(videoId, "nass-videoencoder", videoParam, nullptr);
	blog(LOG_INFO, "videoEncoder %x", videoEncoder);
	if (!videoEncoder)
		return {{"error", "video encoder create failed"}};

	auto video = obs_get_video();
	blog(LOG_INFO, "video %x", video);
	obs_encoder_set_video(videoEncoder, video);

	auto audioParam = obs_data_create_from_json(arg["audio"].dump().c_str());
	auto audioId = arg["audioId"].get_ref<std::string &>().c_str();
	auto audioEncoder = obs_audio_encoder_create(audioId, "nass-audioencoder", audioParam, 0, nullptr);
	blog(LOG_INFO, "audioEncoder %x", audioEncoder);
	if (!audioEncoder)
		return {{"error", "audio encoder create failed"}};

	auto audio = obs_get_audio();
	blog(LOG_INFO, "audio %x", audio);
	obs_encoder_set_audio(audioEncoder, audio);

	obs_output_set_audio_encoder(outputInstance, obs_encoder_get_ref(audioEncoder), 0);
	obs_output_set_video_encoder(outputInstance, obs_encoder_get_ref(videoEncoder));

	auto signalHandler = obs_output_get_signal_handler(outputInstance);
	if (!signalHandler)
		return {{"error", "signal handler get failed"}};

	signal_handler_connect(signalHandler, "starting", &onStarting, nullptr);
	signal_handler_connect(signalHandler, "start", &onStarted, nullptr);
	signal_handler_connect(signalHandler, "stopping", &onStopping, nullptr);
	signal_handler_connect(signalHandler, "stop", &onStopped, nullptr);
	signal_handler_connect(signalHandler, "reconnect", &onReconnect, nullptr);
	signal_handler_connect(signalHandler, "reconnecte_success", &onReconnected, nullptr);
	signal_handler_connect(signalHandler, "deactivate", &onDeactive, nullptr);

	auto result = obs_output_start(outputInstance);
	blog(LOG_INFO, "start %d", result);
	if (!result)
		return {{"error", "output start failed"}};

	output = outputInstance;
	beginTime = std::chrono::steady_clock::now();

	return {{"result", "OK"}};
}

json stop()
{
	if (output)
	{
		blog(LOG_INFO, "stop %x", output);
		obs_output_stop(output);
	}

	return {{"result", "OK"}};
}

json getStatus()
{
	json result;
	bool active = output && obs_output_active(output);
	result["active"] = active;
	result["status"] = statusMessage;
	result["error"] = errorMessage;
	if (active)
	{
		result["duration"] = (std::chrono::steady_clock::now() - beginTime) / std::chrono::seconds(1);
		result["connectTime"] = obs_output_get_connect_time_ms(output);
		result["bytes"] = obs_output_get_total_bytes(output);
		result["frames"] = obs_output_get_total_frames(output);
		result["congestion"] = obs_output_get_congestion(output);
		result["dropped"] = obs_output_get_frames_dropped(output);
	}

	return result;
}

json getInfo()
{
	json result;
	result["version"] = VERSION;
	result["process"] = GetCurrentProcessId();
	return result;
}

//-------------------------------------------

json anyFunction(const json &arg)
{
	auto id = arg["id"].get<std::string>();
	auto functionName = arg["fn"].get<std::string>();
	auto functionArg = arg["arg"];
	json result;

	if (functionName == "enumEncoderTypes")
		result = enumEncoderTypes();
	if (functionName == "start")
		result = start(functionArg);
	if (functionName == "stop")
		result = stop();
	if (functionName == "status")
		result = getStatus();
	if (functionName == "info")
		result = getInfo();

	json response = {{"id", id}, {"res", result}};
	return response;
}

// nameがユニークなので複数起動はできないので注意
#define PIPE_NAME L"\\\\.\\pipe\\NAirSubstream"
#define BUFFER_SIZE 4096

DWORD WINAPI serve(LPVOID lpParam)
{

	HANDLE pipeHandle = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_DUPLEX,
								   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, BUFFER_SIZE, BUFFER_SIZE, 0, NULL);

	if (pipeHandle == INVALID_HANDLE_VALUE)
	{
		blog(LOG_INFO, "Failed to create named pipe.");
		return -1;
	}

	if (!ConnectNamedPipe(pipeHandle, NULL) && GetLastError() != ERROR_PIPE_CONNECTED)
	{
		blog(LOG_INFO, "Failed to connect named pipe.");
		return -1;
	}
	blog(LOG_INFO, "pipe connect");

	char buffer[BUFFER_SIZE + 4];
	DWORD bytesRead;

	while (isActive)
	{
		if (ReadFile(pipeHandle, buffer, BUFFER_SIZE, &bytesRead, NULL))
		{
			if (!bytesRead)
				continue;
			buffer[bytesRead] = '\0';
			// blog(LOG_INFO, "recv %s", buffer);
			auto jsonInput = json::parse(buffer);
			auto jsonResponse = anyFunction(jsonInput);
			auto responseBuffer = jsonResponse.dump();
			WriteFile(pipeHandle, responseBuffer.c_str(), (DWORD)responseBuffer.size(), &bytesRead, NULL);
			// blog(LOG_INFO, "send %s", responseBuffer.c_str());
		}
	}

	CloseHandle(pipeHandle);

	return 0;
}

extern "C"
{
	OBS_DECLARE_MODULE()
	OBS_MODULE_USE_DEFAULT_LOCALE("nair-substream", "en-US")

	bool obs_module_load(void)
	{
		blog(LOG_INFO, "substream plugin loaded successfully %s", VERSION);

		isActive = true;

		DWORD threadId;
		threadHandle = CreateThread(NULL, 0, serve, 0, 0, &threadId);
		if (!threadHandle)
		{
			blog(LOG_INFO, "Failed to create thread.");
			return false;
		}

		return true;
	}

	void obs_module_unload(void)
	{
		isActive = false;
		//	WaitForSingleObject(threadHandle, INFINITE);
		CloseHandle(threadHandle);
		threadHandle = nullptr;
	}
}
