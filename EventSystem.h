// ============================================================
// EventSystem.h — Music-synced timed event system
// ============================================================
// Fires callbacks at specific timestamps relative to a playing
// audio track. Includes an ImGui timeline editor and binary
// save/load so edits persist across sessions.
//
// Usage:
//   1. Call EventSystem::SetTrack(sound) with the ma_sound*
//   2. Register event types via RegisterHandler(type, callback)
//   3. Add events via AddEvent() or the ImGui editor
//   4. Call Tick() every frame from Renderer::Tick
//   5. Call UI() from Renderer::UI
//   6. Call Save() on shutdown / Load() on init
// ============================================================

#pragma once
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

struct ma_sound;

// ── Single timed event ────────────────────────────────────────
struct TimeEvent
{
	float       time      = 0.0f;   // seconds into the track
	std::string type;               // event type key (e.g. "flash", "spawn", "camera_cut")
	float       param1    = 0.0f;   // generic float params — interpret per type
	float       param2    = 0.0f;
	float       param3    = 0.0f;
	std::string strParam;           // optional string payload (scene name, etc.)
	bool        fired     = false;  // reset each playback loop
};

// ── Handler signature ─────────────────────────────────────────
// Receives the event that fired so the handler can read params.
using EventHandler = std::function<void(const TimeEvent&)>;

// ── Event system ──────────────────────────────────────────────
class EventSystem
{
public:
	// ---- Core API ----
	void SetTrack(ma_sound* sound);
	void RegisterHandler(const std::string& type, EventHandler handler);
	void Tick(float deltaTimeMs);
	void Reset();  // mark all events unfired (call on track restart / seek)

	// ---- Event management ----
	void AddEvent(const TimeEvent& e);
	void RemoveEvent(int index);
	void SortEvents();  // by time ascending
	std::vector<TimeEvent>& Events() { return events; }
	const std::vector<TimeEvent>& Events() const { return events; }

	// ---- Persistence ----
	bool Save(const char* path) const;
	bool Load(const char* path);
	void Pause();

	void SetSpeed(float speed);
	void SeekTo(float seconds);
	// ---- ImGui editor ----
	void UI(std::function<void()> resetAccumulator = nullptr);

	// ---- State ----
	float GetPlaybackTime() const;
	bool  IsPlaying() const;

	// Known event types for the combo box
	std::vector<std::string> knownTypes = 
	{
		"fade_in", "fade_out",
		"flash", "camera_cut", "spawn", "color_shift",
		"shake", "bloom_pulse", "scene_change", "custom"
	};

	float playbackSpeed = 1.0f;

private:
	ma_sound* trackedSound = nullptr;
	float lastCursor = 0.0f;
	float tolerance  = 0.05f;
	float cachedTrackLength = 120.0f;  // fallback 2 min

	std::vector<TimeEvent> events;
	std::unordered_map<std::string, EventHandler> handlers;

	// Editor state
	int   selectedEvent = -1;
	bool  editorOpen    = true;
	float zoomLevel     = 100.0f;  // pixels per second in timeline
	float scrollOffset  = 0.0f;

	bool enabled = false;
};
