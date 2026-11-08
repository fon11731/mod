// ============================================================
// 把下面改动合并进 src/app/dllmain.cpp
// ============================================================

// 1) 顶部增加头文件
#include "web/server.hpp"

// 2) 在 start() 里，install_overlay 成功之后、install_game_hooks 之前插入：

    // ---------- WebUI ----------
    {
        training::web::Config web_cfg;
        web_cfg.enabled = true;   // 可改成从 ini 读取
        web_cfg.port = 27182;
        try {
            training::web::initialize_web_server(training::game_session(), web_cfg);
        } catch (const std::exception& e) {
            training::log(std::string("WebUI init failed: ") + e.what());
        }
    }
    // ---------------------------

// 完整参考 start() 结构（合并后大致如下）：
/*
DWORD WINAPI start(void* module) {
    training::open_log(static_cast<HMODULE>(module));
    std::string config_error;
    if (!training::initialize_preferences(static_cast<HMODULE>(module), config_error))
        training::log(config_error);

    training::log(std::string("Training Master ") + training::about::build_date +
                  " starting (disabled by default)");

    const auto mh = MH_Initialize();
    if (mh != MH_OK) {
        training::log("MinHook initialization failed");
        return 1;
    }

    HMODULE game{};
    for (int i = 0; i < 1200 && !game; ++i) {
        game = GetModuleHandleW(L"soundvoltex.dll");
        if (!game) Sleep(100);
    }
    if (!game) {
        training::log("soundvoltex.dll was not loaded within 120 seconds");
        return 2;
    }

    std::string reason;
    if (!training::install_overlay(training::game_session(), reason)) {
        training::log(reason);
        return 3;
    }
    training::log(reason);

    // WebUI
    {
        training::web::Config web_cfg;
        web_cfg.enabled = true;
        web_cfg.port = 27182;
        try {
            training::web::initialize_web_server(training::game_session(), web_cfg);
        } catch (const std::exception& e) {
            training::log(std::string("WebUI init failed: ") + e.what());
        }
    }

    const bool ok = training::install_game_hooks(game, reason);
    training::set_overlay_status(reason, !ok);
    training::log(reason);
    return ok ? 0 : 4;
}
*/
