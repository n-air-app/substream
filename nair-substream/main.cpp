// NAir Substream - OBS Studioプラグイン
//
// このプラグインはOBS Studioのストリーミング機能をNAirアプリケーション経由で
// リモート制御するための機能を提供します。名前付きパイプを使用して通信し、
// JSONフォーマットでコマンドを受け取り、ストリーミングの開始/停止や
// ステータス取得などの操作を実行します。

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

// グローバル変数
obs_output *output = nullptr;						  // ストリーミング出力インスタンス
std::chrono::steady_clock::time_point beginTime = {}; // ストリーミング開始時間
std::string statusMessage = "stopped";				  // 現在のステータスメッセージ
std::string errorMessage = "";						  // エラーメッセージ（存在する場合）

bool isPluginActive = false;   // プラグインがアクティブかどうか
HANDLE threadHandle = nullptr; // 通信スレッドのハンドル

bool isBusy = false;	  // ストリーミング処理中かどうか
bool isStreaming = false; // ストリーミング中かどうか

//-------------------------------------------
// OBS出力イベントハンドラー
//-------------------------------------------

// ストリーミングの開始処理中
void onStarting(void *x, calldata_t *)
{
	statusMessage = "starting";
	errorMessage = "";
	isBusy = true;
	isStreaming = false;
}

// ストリーミングの開始完了
void onStarted(void *x, calldata_t *)
{
	statusMessage = "started";
	isBusy = false;
	isStreaming = true;
}

// ストリーミングの停止処理中
void onStopping(void *x, calldata_t *)
{
	statusMessage = "stopping";
	isBusy = true;
	isStreaming = true;
}

// ストリーミングの停止完了（エラーコードの処理も含む）
void onStopped(void *x, calldata_t *param)
{
	statusMessage = "stopped";
	errorMessage = "";
	isBusy = false;
	isStreaming = false;

	// エラーコードに応じたメッセージを設定
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

// 再接続試行中
void onReconnect(void *x, calldata_t *)
{
	statusMessage = "reconnect";
}

// 再接続成功
void onReconnected(void *x, calldata_t *)
{
	statusMessage = "reconnected";
}

// 出力非アクティブ化
void onDeactive(void *x, calldata_t *)
{
	statusMessage = "deactive";
}

//-------------------------------------------
// 機能実装
//-------------------------------------------

// 利用可能なエンコーダータイプの列挙
//
// @return JSONオブジェクト - 音声および動画エンコーダーの種類と名前を含む
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

// 現在のストリーミング出力とリソースを解放
// エンコーダー、サービス、シグナルハンドラなどを全て解放します
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
		signal_handler_disconnect(signalHandler, "reconnect_success", &onReconnected, nullptr);
		signal_handler_disconnect(signalHandler, "deactivate", &onDeactive, nullptr);
	}

	obs_output_release(output);
	output = nullptr;
}

// ストリーミングを開始
//
// @param arg JSONオブジェクト - 出力、サービス、エンコーダーの設定を含む
// @return JSONオブジェクト - 結果またはエラーメッセージ
json start(json &arg)
{
	blog(LOG_INFO, "start requested");
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

	auto videoInfo = obs_get_video_info_by_index2(0);
	auto renderingMode = obs_get_video_rendering_mode();
	auto videoMix = videoInfo ? obs_video_mix_get(videoInfo, renderingMode) : nullptr;
	if (!videoMix)
		return {{"error", "video mix get failed"}};
	blog(LOG_INFO, "selected video rendering mode %d", renderingMode);
	obs_encoder_set_video_mix(videoEncoder, videoMix);

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
	signal_handler_connect(signalHandler, "reconnect_success", &onReconnected, nullptr);
	signal_handler_connect(signalHandler, "deactivate", &onDeactive, nullptr);

	auto result = obs_output_start(outputInstance);
	blog(LOG_INFO, "start %d", result);
	if (!result)
		return {{"error", "output start failed"}};

	output = outputInstance;
	beginTime = std::chrono::steady_clock::now();

	return {{"result", "OK"}};
}

// ストリーミングを停止
//
// @return JSONオブジェクト - 操作結果
json stop()
{
	if (output)
	{
		blog(LOG_INFO, "stop %x", output);
		obs_output_stop(output);
	}

	return {{"result", "OK"}};
}

// 現在のストリーミングステータスを取得
// アクティブ状態、エラー、統計情報を含む
//
// @return JSONオブジェクト - ステータス情報
json getStatus()
{
	static auto lastLogTime = std::chrono::steady_clock::time_point{};
	json result;
	bool active = output && obs_output_active(output);
	result["active"] = active;
	result["status"] = statusMessage;
	result["error"] = errorMessage;
	result["busy"] = isBusy;
	result["streaming"] = isStreaming;
	if (active)
	{
		result["duration"] = (std::chrono::steady_clock::now() - beginTime) / std::chrono::seconds(1);
		result["connectTime"] = obs_output_get_connect_time_ms(output);
		result["bytes"] = obs_output_get_total_bytes(output);
		result["frames"] = obs_output_get_total_frames(output);
		result["congestion"] = obs_output_get_congestion(output);
		result["dropped"] = obs_output_get_frames_dropped(output);
	}

	auto now = std::chrono::steady_clock::now();
	if (now - lastLogTime >= std::chrono::seconds(5))
	{
		blog(LOG_INFO, "substream status active %d status %s frames %llu bytes %llu dropped %d",
		     active, statusMessage.c_str(),
		     active ? (unsigned long long)obs_output_get_total_frames(output) : 0,
		     active ? (unsigned long long)obs_output_get_total_bytes(output) : 0,
		     active ? obs_output_get_frames_dropped(output) : 0);
		lastLogTime = now;
	}

	return result;
}

// プラグイン情報を取得
//
// @return JSONオブジェクト - バージョンとプロセスID
json getInfo()
{
	json result;
	result["version"] = VERSION;
	result["process"] = GetCurrentProcessId();
	return result;
}

//-------------------------------------------
// コマンド処理
//-------------------------------------------

// 受信したJSONコマンドを解析して適切な関数を呼び出す
// @param arg JSONオブジェクト - id, fn(関数名), arg(引数)を含む
// @return JSONオブジェクト - コマンドの実行結果
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

// 名前付きパイプの設定 - 名前がユニークなので複数起動はできないことに注意
#define PIPE_NAME L"\\\\.\\pipe\\NAirSubstream"
#define BUFFER_SIZE 4096

// 名前付きパイプを使用してコマンドを受け付けるスレッド関数
// @param lpParam スレッドパラメータ（使用せず）
// @return スレッド終了コード
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
	std::string receiveBuffer; // 受信データを蓄積するバッファ

	while (isPluginActive)
	{
		if (ReadFile(pipeHandle, buffer, BUFFER_SIZE, &bytesRead, NULL))
		{
			if (!bytesRead)
				continue;

			// 受信データをバッファに追加
			receiveBuffer.append(buffer, bytesRead);

			// 改行区切りでJSONを処理
			size_t pos;
			while ((pos = receiveBuffer.find('\n')) != std::string::npos)
			{
				std::string jsonLine = receiveBuffer.substr(0, pos);
				receiveBuffer.erase(0, pos + 1);

				if (jsonLine.empty())
					continue;

				try
				{
					// blog(LOG_INFO, "recv %s", jsonLine.c_str());
					auto jsonInput = json::parse(jsonLine);
					auto jsonResponse = anyFunction(jsonInput);
					auto responseBuffer = jsonResponse.dump(-1) + "\n";
					WriteFile(pipeHandle, responseBuffer.c_str(), (DWORD)responseBuffer.size(), &bytesRead, NULL);
					// blog(LOG_INFO, "send %s", responseBuffer.c_str());
				}
				catch (const json::parse_error &e)
				{
					blog(LOG_WARNING, "JSON parse error: %s", e.what());
				}
			}
		}
	}

	CloseHandle(pipeHandle);

	return 0;
}

// OBSプラグインのエントリーポイント
extern "C"
{
	OBS_DECLARE_MODULE()
	OBS_MODULE_USE_DEFAULT_LOCALE("nair-substream", "en-US")

	// プラグインロード時に呼ばれる関数
	// 通信スレッドを起動し、名前付きパイプサーバーを開始する
	// @return 初期化成功ならtrue
	bool obs_module_load(void)
	{
		blog(LOG_INFO, "substream plugin loaded successfully %s", VERSION);

		isPluginActive = true;

		DWORD threadId;
		threadHandle = CreateThread(NULL, 0, serve, 0, 0, &threadId);
		if (!threadHandle)
		{
			blog(LOG_INFO, "Failed to create thread.");
			return false;
		}

		return true;
	}

	// プラグインアンロード時に呼ばれる関数
	// リソースを解放する
	void obs_module_unload(void)
	{
		isPluginActive = false;
		//	WaitForSingleObject(threadHandle, INFINITE);
		CloseHandle(threadHandle);
		threadHandle = nullptr;
	}
}
