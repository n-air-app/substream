#define STRICT
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _USE_MATH_DEFINES

#include <Windows.h>
#include <obs-module.h>
#include "../thirdparty/json.hpp"
using json = nlohmann::json;

#pragma comment(lib, "obs.lib")

obs_output *output = nullptr;
std::chrono::steady_clock::time_point begin_time = {};
std::string statusMessage = "";
std::string errorMessage = "";

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

	json r =
		{{"encoders", {{"audio", audio}, {"video", video}}}};
	return r;
}

void release()
{
	if (!output)
		return;
	obs_output_stop(output);

	auto venc = obs_output_get_video_encoder(output);
	if (venc)
	{
		obs_output_set_video_encoder(output, nullptr);
		obs_encoder_release(venc);
	}

	auto aenc = obs_output_get_audio_encoder(output, 0);
	if (aenc)
	{
		obs_output_set_audio_encoder(output, nullptr, 0);
		obs_encoder_release(aenc);
	}

	auto service = obs_output_get_service(output);
	if (service)
	{
		obs_output_set_service(output, nullptr);
		obs_service_release(service);
	}

	auto signal = obs_output_get_signal_handler(output);
	if (signal)
	{
		signal_handler_disconnect(signal, "starting", &onStarting, nullptr);
		signal_handler_disconnect(signal, "start", &onStarted, nullptr);
		signal_handler_disconnect(signal, "stopping", &onStopping, nullptr);
		signal_handler_disconnect(signal, "stop", &onStopped, nullptr);
		signal_handler_disconnect(signal, "reconnect", &onReconnect, nullptr);
		signal_handler_disconnect(signal, "reconnected_success", &onReconnected, nullptr);
		signal_handler_disconnect(signal, "deactivate", &onDeactive, nullptr);
	}

	obs_output_release(output);
	output = nullptr;
}

json start(json &arg)
{
	blog(LOG_INFO, "start %s", arg.dump().c_str());
	release();

	obs_data *output_param = obs_data_create_from_json(arg["output"].dump().c_str());
	auto op = obs_output_create("rtmp_output", "nass-output", output_param, nullptr);
	blog(LOG_INFO, "output %x ", op);
	if (!op)
		return {{"error", "output create failed"}};

	auto service_param = obs_data_create_from_json(arg["service"].dump().c_str());
	auto service = obs_service_create("rtmp_custom", "nass-service", service_param, nullptr);
	blog(LOG_INFO, "service %x", service);
	if (!service)
		return {{"error", "service create failed"}};
	obs_output_set_service(op, service);

	auto venc_param = obs_data_create_from_json(arg["video"].dump().c_str());
	auto venc_id = arg["videoId"].get_ref<std::string &>().c_str();
	auto venc = obs_video_encoder_create(venc_id, "nass-videoencoder", venc_param, nullptr);
	blog(LOG_INFO, "venc %x", venc);
	if (!venc)
		return {{"error", "video encoder create failed"}};

	auto video = obs_get_video();
	blog(LOG_INFO, "video %x", video);
	obs_encoder_set_video(venc, video);

	auto aenc_param = obs_data_create_from_json(arg["audio"].dump().c_str());
	auto aenc_id = arg["audioId"].get_ref<std::string &>().c_str();
	auto aenc = obs_audio_encoder_create(aenc_id, "nass-audioencoder", aenc_param, 0, nullptr);
	blog(LOG_INFO, "aenc %x", aenc);
	if (!aenc)
		return {{"error", "audio encoder create failed"}};

	auto audio = obs_get_audio();
	blog(LOG_INFO, "audio %x", audio);
	obs_encoder_set_audio(aenc, audio);

	obs_output_set_audio_encoder(op, obs_encoder_get_ref(aenc), 0);
	obs_output_set_video_encoder(op, obs_encoder_get_ref(venc));

	auto signal = obs_output_get_signal_handler(op);
	if (!signal)
		return {{"error", "signal handler get failed"}};

	signal_handler_connect(signal, "starting", &onStarting, nullptr);
	signal_handler_connect(signal, "start", &onStarted, nullptr);
	signal_handler_connect(signal, "stopping", &onStopping, nullptr);
	signal_handler_connect(signal, "stop", &onStopped, nullptr);
	signal_handler_connect(signal, "reconnect", &onReconnect, nullptr);
	signal_handler_connect(signal, "reconnecte_success", &onReconnected, nullptr);
	signal_handler_connect(signal, "deactivate", &onDeactive, nullptr);

	auto res = obs_output_start(op);
	blog(LOG_INFO, "start %d", res);
	if (!res)
		return {{"error", "output start failed"}};

	output = op;
	begin_time = std::chrono::steady_clock::now();

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
	json r;
	bool active = output && obs_output_active(output);
	r["active"] = active;
	r["status"] = statusMessage;
	r["error"] = errorMessage;
	r["process"] = GetCurrentProcessId();
	if (active)
	{
		r["duration"] =(std::chrono::steady_clock::now() - begin_time) / std::chrono::seconds(1);
		r["connect_time"] = obs_output_get_connect_time_ms(output);
		r["bytes"] = obs_output_get_total_bytes(output);
		r["frames"] = obs_output_get_total_frames(output);
		r["congestion"] = obs_output_get_congestion(output);
		r["dropped"] = obs_output_get_frames_dropped(output);
	}

	return r;
}

//-------------------------------------------

json anyFunction(const json &arg)
{
	auto id = arg["id"].get<std::string>();
	auto fn = arg["fn"].get<std::string>();
	auto a = arg["arg"];
	json r;

	if (fn == "enumEncoderTypes")
		r = enumEncoderTypes();
	if (fn == "start")
		r = start(a);
	if (fn == "stop")
		r = stop();
	if (fn == "status")
		r = getStatus();

	json result = {{"id", id}, {"res", r}};
	return result;
}

// nameがユニークなので複数起動はできないので注意
#define PIPE_NAME "\\\\.\\pipe\\NAirSubstream"
#define BUFFER_SIZE 4096

DWORD WINAPI Serve(LPVOID lpParam)
{

	HANDLE hPipe = CreateNamedPipeA(PIPE_NAME, PIPE_ACCESS_DUPLEX,
									PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, BUFFER_SIZE, BUFFER_SIZE, 0, NULL);

	if (hPipe == INVALID_HANDLE_VALUE)
	{
		blog(LOG_INFO, "Failed to create named pipe.");
		return -1;
	}

	if (!ConnectNamedPipe(hPipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED)
	{
		blog(LOG_INFO, "Failed to connect named pipe.");
		return -1;
	}
	blog(LOG_INFO, "pipe connect");

	char buffer[BUFFER_SIZE + 4];
	DWORD bytesRead;

	while (true)
	{
		if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL))
		{
			buffer[bytesRead] = '\0';
			// blog(LOG_INFO, "recv %s", buffer);
			auto j = json::parse(buffer);
			auto r = anyFunction(j);
			auto rb = r.dump();
			WriteFile(hPipe, rb.c_str(), (DWORD)rb.size(), &bytesRead, NULL);
			// blog(LOG_INFO, "send %s", rb.c_str());
		}
	}

	CloseHandle(hPipe);

	return 0;
}

extern "C"
{
	OBS_DECLARE_MODULE()
	OBS_MODULE_USE_DEFAULT_LOCALE("nair-substream", "en-US")

	bool obs_module_load(void)
	{
		blog(LOG_INFO, "sssss plugin loaded successfully");

		DWORD threadId;
		HANDLE hThread = CreateThread(NULL, 0, Serve, 0, 0, &threadId);

		return true;
	}

	void obs_module_unload(void)
	{
	}
}
