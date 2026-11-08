#include "web/server.hpp"
#include "core/autoplay_mode.hpp"
#include "core/playback_rate.hpp"
#include "platform/log.hpp"
#include "ui/preferences.hpp"

#include <httplib.h>

#include <cmath>
#include <mutex>
#include <sstream>
#include <string>

namespace training::web {
namespace {

// ---------- 简易 JSON 工具 ----------
std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    return out;
}

std::string state_to_json(const State& s) {
    std::ostringstream o;
    o << std::boolalpha;
    o << '{'
      << "\"playing\":" << s.playing << ','
      << "\"enabled\":" << s.enabled << ','
      << "\"paused\":" << s.paused << ','
      << "\"position\":" << s.position << ','
      << "\"duration\":" << s.track.duration << ','
      << "\"in\":" << s.in << ','
      << "\"out\":" << s.out << ','
      << "\"loop\":" << s.loop << ','
      << "\"repeat\":" << s.repeat << ','
      << "\"rate\":" << s.rate << ','
      << "\"autoplay\":" << s.autoplay << ','
      << "\"autoplay_mode\":" << static_cast<int>(s.autoplay_mode) << ','
      // 🌟 新增：前端读取当前策略
      << "\"upload_mode\":" << static_cast<int>(preferences().snapshot().autoplay_upload_mode) << ','
      << "\"lane_speed_locked\":" << s.lane_speed_locked << ','
      << "\"camera_locked\":" << s.camera_locked << ','
      << "\"hide_song_visuals\":" << s.hide_song_visuals << ','
      << "\"ready\":" << s.ready << ','
      << "\"generation\":" << s.generation << ','
      << "\"message\":\"" << json_escape(s.message) << "\","
      << "\"chart\":\"" << json_escape(s.track.chart) << "\""
      << '}';
    return o.str();
}

bool parse_action(const std::string& name, Action& out) {
    if (name == "Enable")              { out = Action::Enable; return true; }
    if (name == "Disable")             { out = Action::Disable; return true; }
    if (name == "Pause")               { out = Action::Pause; return true; }
    if (name == "Resume")              { out = Action::Resume; return true; }
    if (name == "Seek")                { out = Action::Seek; return true; }
    if (name == "SetIn")               { out = Action::SetIn; return true; }
    if (name == "SetOut")              { out = Action::SetOut; return true; }
    if (name == "SetLoop")             { out = Action::SetLoop; return true; }
    if (name == "SetRate")             { out = Action::SetRate; return true; }
    if (name == "Restart")             { out = Action::Restart; return true; }
    if (name == "Finish")              { out = Action::Finish; return true; }
    if (name == "SetAutoplay")         { out = Action::SetAutoplay; return true; }
    if (name == "SetAutoplayMode")     { out = Action::SetAutoplayMode; return true; }
    if (name == "SetLaneSpeedLocked")  { out = Action::SetLaneSpeedLocked; return true; }
    if (name == "SetCameraLocked")     { out = Action::SetCameraLocked; return true; }
    if (name == "SetHideSongVisuals")  { out = Action::SetHideSongVisuals; return true; }
    if (name == "CycleLoop")           { out = Action::CycleLoop; return true; }
    if (name == "ReturnToStart")       { out = Action::ReturnToStart; return true; }
    if (name == "TogglePlayback")      { out = Action::TogglePlayback; return true; }
    return false;
}

bool extract_string(const std::string& body, const char* key, std::string& out) {
    const std::string pattern = std::string("\"") + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;
    pos = body.find('"', pos + 1);
    if (pos == std::string::npos) return false;
    ++pos;
    auto end = body.find('"', pos);
    if (end == std::string::npos) return false;
    out = body.substr(pos, end - pos);
    return true;
}

bool extract_number(const std::string& body, const char* key, float& out) {
    const std::string pattern = std::string("\"") + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) ++pos;
    char* end = nullptr;
    float v = std::strtof(body.c_str() + pos, &end);
    if (end == body.c_str() + pos || !std::isfinite(v)) return false;
    out = v;
    return true;
}

// ---------- 前端页面 ----------
const char* kIndexHtml = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Training Master</title>
<style>
:root {
  --bg: #0c1014;
  --card: #141c22;
  --border: #243038;
  --accent: #3dd6b0;
  --accent-dim: #2a9e84;
  --text: #e4eef5;
  --muted: #7d8f9c;
  --danger: #e86a6a;
  --radius: 10px;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  background: var(--bg);
  color: var(--text);
  font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
  padding: 14px;
  max-width: 520px;
  margin: 0 auto;
  line-height: 1.4;
  -webkit-tap-highlight-color: transparent;
}
h1 { font-size: 1.15rem; color: var(--accent); margin-bottom: 4px; letter-spacing: 0.02em; }
.sub { font-size: 12px; color: var(--muted); margin-bottom: 14px; }
.card { background: var(--card); border: 1px solid var(--border); border-radius: var(--radius); padding: 14px; margin-bottom: 12px; }
.card h2 { font-size: 12px; text-transform: uppercase; letter-spacing: 0.06em; color: var(--accent); margin-bottom: 10px; }
.status { font-size: 13px; color: var(--muted); min-height: 1.2em; margin-bottom: 6px; }
.chart { font-size: 13px; color: var(--text); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; margin-bottom: 8px; }
.pos { font-variant-numeric: tabular-nums; font-size: 1.55rem; font-weight: 600; margin-bottom: 8px; }
input[type=range] { width: 100%; accent-color: var(--accent); margin: 6px 0 10px; }
.row { display: flex; flex-wrap: wrap; gap: 8px; }
button { flex: 1 1 auto; min-width: 72px; background: #1a262e; color: var(--text); border: 1px solid var(--border); border-radius: 8px; padding: 11px 10px; font-size: 14px; cursor: pointer; user-select: none; }
button:active { transform: scale(0.97); }
button.on { background: var(--accent-dim); border-color: var(--accent); color: #06120f; font-weight: 600; }
button.primary { background: var(--accent); border-color: var(--accent); color: #06120f; font-weight: 700; }
button.danger { background: #3a1e1e; border-color: #5a3030; color: var(--danger); }
.rate-val { font-variant-numeric: tabular-nums; color: var(--accent); font-weight: 600; }
.hint { font-size: 11px; color: var(--muted); margin-top: 8px; }
.offline { color: var(--danger); }
select { width: 100%; background: #1a262e; color: var(--text); border: 1px solid var(--border); border-radius: 8px; padding: 10px; font-size: 14px; margin-top: 4px; }
</style>
</head>
<body>
  <h1>Training Master</h1>
  <div class="sub">WebUI · 局域网控制</div>

  <div class="card">
    <div class="status" id="msg">连接中…</div>
    <div class="chart" id="chart">—</div>
    <div class="pos" id="pos">--:-- / --:--</div>
    <input type="range" id="seek" min="0" max="100" value="0" step="0.05">
    <div class="row">
      <button id="btn-enable">开启训练</button>
      <button id="btn-pause">暂停</button>
      <button id="btn-loop">循环</button>
    </div>
  </div>

  <div class="card">
    <h2>速度 <span class="rate-val" id="rate-val">1.00x</span></h2>
    <input type="range" id="rate" min="0.25" max="2.0" value="1.0" step="0.05">
    <div class="row">
      <button data-rate="0.50">0.50x</button>
      <button data-rate="0.75">0.75x</button>
      <button data-rate="1.00">1.00x</button>
      <button data-rate="1.25">1.25x</button>
      <button data-rate="1.50">1.50x</button>
    </div>
  </div>

  <div class="card">
    <h2>循环点</h2>
    <div class="row">
      <button id="btn-in">设为 IN</button>
      <button id="btn-out">设为 OUT</button>
      <button id="btn-restart">回开头</button>
    </div>
  </div>

  <div class="card">
    <h2>Autoplay 与上传策略 (三档)</h2>
    <select id="sel-mode">
      <option value="0">Auto + 上传成绩 (测试)</option>
      <option value="1">Auto + 不上传成绩 (训练)</option>
      <option value="2">Manual (关闭 Auto) + 上传成绩</option>
    </select>
  </div>

  <div class="card">
    <h2>功能</h2>
    <div class="row">
      <button id="btn-autoplay" style="display:none">AutoPlay (由上方策略控制)</button>
      <button id="btn-lane">锁流速</button>
      <button id="btn-camera">锁镜头</button>
    </div>
    <div class="row" style="margin-top:8px">
      <button id="btn-visuals">隐藏演出</button>
      <button id="btn-finish" class="danger">立即结束</button>
    </div>
  </div>

  <div class="hint">手机浏览器建议「添加到主屏幕」。每 250ms 刷新状态。</div>

<script>
const $ = id => document.getElementById(id);
let state = {};
let seeking = false;

function fmt(t) {
  if (!isFinite(t) || t < 0) return '--:--';
  const m = Math.floor(t / 60);
  const s = Math.floor(t % 60);
  return m + ':' + String(s).padStart(2, '0');
}

function setOn(btn, on) {
  if (on) btn.classList.add('on');
  else btn.classList.remove('on');
}

async function refresh() {
  try {
    const r = await fetch('/api/state', { cache: 'no-store' });
    if (!r.ok) throw new Error('bad status');
    state = await r.json();
    $('msg').textContent = state.message || '';
    $('msg').classList.remove('offline');
    $('chart').textContent = state.chart || '—';
    $('pos').textContent = fmt(state.position) + ' / ' + fmt(state.duration);
    const dur = state.duration > 0 ? state.duration : 100;
    $('seek').max = dur;
    if (!seeking) $('seek').value = state.position || 0;
    $('rate').value = state.rate || 1;
    $('rate-val').textContent = (Number(state.rate) || 1).toFixed(2) + 'x';

    $('btn-enable').textContent = state.enabled ? '关闭训练' : '开启训练';
    setOn($('btn-enable'), !!state.enabled);
    $('btn-pause').textContent = state.paused ? '继续' : '暂停';
    setOn($('btn-pause'), !!state.paused);
    setOn($('btn-loop'), !!state.loop);
    setOn($('btn-lane'), !!state.lane_speed_locked);
    setOn($('btn-camera'), !!state.camera_locked);
    setOn($('btn-visuals'), !!state.hide_song_visuals);
    
    // 更新下拉框状态
    $('sel-mode').value = state.upload_mode;
  } catch (e) {
    $('msg').textContent = '连接失败 · 请确认游戏已启动且 WebUI 已开启';
    $('msg').classList.add('offline');
  }
}

function cmd(action, value = 0) {
  fetch('/api/command', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ action, value })
  }).then(() => setTimeout(refresh, 80));
}

function set_mode(mode) {
  fetch('/api/upload_mode', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ mode })
  }).then(() => setTimeout(refresh, 80));
}

$('btn-enable').onclick = () => cmd(state.enabled ? 'Disable' : 'Enable');
$('btn-pause').onclick = () => cmd(state.paused ? 'Resume' : 'Pause');
$('btn-loop').onclick = () => cmd('CycleLoop');
$('btn-in').onclick = () => cmd('SetIn', state.position || 0);
$('btn-out').onclick = () => cmd('SetOut', state.position || 0);
$('btn-restart').onclick = () => cmd('ReturnToStart');
$('btn-lane').onclick = () => cmd('SetLaneSpeedLocked', state.lane_speed_locked ? 0 : 1);
$('btn-camera').onclick = () => cmd('SetCameraLocked', state.camera_locked ? 0 : 1);
$('btn-visuals').onclick = () => cmd('SetHideSongVisuals', state.hide_song_visuals ? 0 : 1);
$('btn-finish').onclick = () => {
  if (confirm('确定立即结束本曲？')) cmd('Finish');
};

$('sel-mode').addEventListener('change', e => set_mode(+e.target.value));

$('seek').addEventListener('pointerdown', () => { seeking = true; });
$('seek').addEventListener('pointerup', e => {
  seeking = false;
  cmd('Seek', +e.target.value);
});
$('seek').addEventListener('change', e => {
  seeking = false;
  cmd('Seek', +e.target.value);
});

$('rate').addEventListener('change', e => cmd('SetRate', +e.target.value));
document.querySelectorAll('[data-rate]').forEach(b => {
  b.onclick = () => cmd('SetRate', +b.dataset.rate);
});

setInterval(refresh, 250);
refresh();
</script>
</body>
</html>
)HTML";

} // namespace

// ---------- Impl ----------
struct Server::Impl {
    httplib::Server svr;
};

Server::Server(Session& session) : session_(session), impl_(std::make_unique<Impl>()) {}

Server::~Server() { stop(); }

void Server::configure(const Config& config) {
    if (running_) return;
    port_ = config.port > 0 && config.port < 65536 ? config.port : 27182;
}

std::string Server::listen_address() const {
    return "0.0.0.0:" + std::to_string(port_);
}

void Server::run() {
    auto& svr = impl_->svr;

    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"},
        {"Cache-Control", "no-store"}
    });

    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    svr.Get("/api/state", [this](const httplib::Request&, httplib::Response& res) {
        const auto s = session_.snapshot();
        res.set_content(state_to_json(s), "application/json; charset=utf-8");
    });

    svr.Post("/api/command", [this](const httplib::Request& req, httplib::Response& res) {
        std::string action_name;
        float value = 0.f;
        if (!extract_string(req.body, "action", action_name)) {
            res.status = 400;
            res.set_content(R"({"ok":false,"error":"missing action"})", "application/json");
            return;
        }
        extract_number(req.body, "value", value);

        Action action;
        if (!parse_action(action_name, action)) {
            res.status = 400;
            res.set_content(R"({"ok":false,"error":"unknown action"})", "application/json");
            return;
        }

        if (action == Action::SetRate) {
            if (value < min_playback_rate) value = min_playback_rate;
            if (value > max_playback_rate) value = max_playback_rate;
        }

        const auto s = session_.snapshot();
        session_.submit({s.generation, action, value});
        res.set_content(R"({"ok":true})", "application/json");
    });

    // 🌟 新增：修改三档开关的 API
    svr.Post("/api/upload_mode", [](const httplib::Request& req, httplib::Response& res) {
        float mode_val;
        if (extract_number(req.body, "mode", mode_val)) {
            auto prefs = training::preferences().snapshot();
            prefs.autoplay_upload_mode = static_cast<training::AutoplayUploadMode>(static_cast<int>(mode_val));
            std::string err;
            training::preferences().save(prefs, err);
            res.set_content(R"({"ok":true})", "application/json");
        } else {
            res.status = 400; 
            res.set_content(R"({"ok":false,"error":"missing mode"})", "application/json");
        }
    });

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(kIndexHtml, "text/html; charset=utf-8");
    });

    svr.Get("/api/ping", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"ok":true,"name":"Training Master WebUI"})", "application/json");
    });

    running_ = true;
    log(std::string("WebUI listening on http://") + listen_address() + " (LAN accessible)");
    const bool ok = svr.listen("0.0.0.0", port_);
    if (!ok && should_run_) {
        log("WebUI failed to bind port " + std::to_string(port_));
    }
    running_ = false;
}

void Server::start() {
    if (running_ || should_run_) return;
    should_run_ = true;
    impl_ = std::make_unique<Impl>();
    thread_ = std::make_unique<std::thread>([this] { run(); });
}

void Server::stop() {
    should_run_ = false;
    if (impl_) {
        impl_->svr.stop();
    }
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
    thread_.reset();
    running_ = false;
}

Server& web_server() {
    static Server* instance = nullptr;
    struct Holder {
        std::unique_ptr<Server> ptr;
    };
    static Holder holder;
    if (!holder.ptr) {
        throw std::runtime_error("web_server not initialized");
    }
    return *holder.ptr;
}

void initialize_web_server(Session& session, const Config& config) {
    struct Holder {
        std::unique_ptr<Server> ptr;
    };
    static Holder holder;
    if (holder.ptr) return;
    holder.ptr = std::make_unique<Server>(session);
    holder.ptr->configure(config);
    if (config.enabled) {
        holder.ptr->start();
    } else {
        log("WebUI disabled by config");
    }
}

} // namespace training::web