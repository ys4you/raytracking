
#pragma once
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

struct ma_sound;

struct TimeEvent
{
	float       time      = 0.0f;
	std::string type;
	float       param1    = 0.0f;
	float       param2    = 0.0f;
	float       param3    = 0.0f;
	std::string strParam;
	bool        fired     = false;
};

using EventHandler = std::function<void(const TimeEvent&)>;

class EventSystem
{
public:
	/// <summary>Associates the event system with a playing audio track.</summary>
	void SetTrack(ma_sound* sound);
	/// <summary>Registers a callback for a named event type.</summary>
	void RegisterHandler(const std::string& type, EventHandler handler);
	/// <summary>Advances playback state and fires due events.</summary>
	void Tick(float deltaTimeMs);
	/// <summary>Marks all scheduled events as not fired.</summary>
	void Reset();

	/// <summary>Adds a timed event to the schedule.</summary>
	void AddEvent(const TimeEvent& e);
	/// <summary>Removes a timed event by index.</summary>
	void RemoveEvent(int index);
	/// <summary>Sorts scheduled events by ascending time.</summary>
	void SortEvents();
	/// <summary>Returns mutable access to all scheduled events.</summary>
	std::vector<TimeEvent>& Events() { return events; }
	/// <summary>Returns read-only access to all scheduled events.</summary>
	const std::vector<TimeEvent>& Events() const { return events; }

	/// <summary>Saves events to a binary file.</summary>
	bool Save(const char* path) const;
	/// <summary>Loads events from a binary file.</summary>
	bool Load(const char* path);
	/// <summary>Pauses the tracked audio playback.</summary>
	void Pause();

	/// <summary>Sets playback speed multiplier.</summary>
	void SetSpeed(float speed);
	/// <summary>Seeks playback to the given time in seconds.</summary>
	void SeekTo(float seconds);
	/// <summary>Draws the event editor user interface.</summary>
	void UI(std::function<void()> resetAccumulator = nullptr);

	/// <summary>Returns current playback cursor in seconds.</summary>
	float GetPlaybackTime() const;
	/// <summary>Returns true when the tracked audio is playing.</summary>
	bool  IsPlaying() const;

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
	float cachedTrackLength = 120.0f;

	std::vector<TimeEvent> events;
	std::unordered_map<std::string, EventHandler> handlers;

	int   selectedEvent = -1;
	bool  editorOpen    = true;
	float zoomLevel     = 100.0f;
	float scrollOffset  = 0.0f;

	bool enabled = false;
};
