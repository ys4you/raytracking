// ============================================================
// EventSystem.cpp
// ============================================================

#include "template.h"
#include "EventSystem.h"
#include "Core/Audio/AudioSystem.h"
#include "miniaudio.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

// ── Track binding ─────────────────────────────────────────────

void EventSystem::SetTrack(ma_sound* sound)
{
	trackedSound = sound;
	lastCursor = 0.0f;
	Reset();

	// Cache track length once — ma_sound_get_length_in_seconds
	// does a brute-force decode scan for MP3s and can crash if
	// called repeatedly from the UI thread.
	cachedTrackLength = 120.0f;
	if (trackedSound)
	{
		float len = 0;
		ma_result result = ma_sound_get_length_in_seconds(trackedSound, &len);
		if (result == MA_SUCCESS && len > 0)
			cachedTrackLength = len;
		else
			printf("[EventSystem] Could not query track length (code %d), using %.0fs default\n", result, cachedTrackLength);
	}
}

float EventSystem::GetPlaybackTime() const
{
	if (!trackedSound) return 0.0f;
	float cursor = 0.0f;
	ma_sound_get_cursor_in_seconds(trackedSound, &cursor);
	return cursor;
}

bool EventSystem::IsPlaying() const
{
	if (!trackedSound) return false;
	return ma_sound_is_playing(trackedSound);
}

// ── Handler registration ──────────────────────────────────────

void EventSystem::RegisterHandler(const std::string& type, EventHandler handler)
{
	handlers[type] = handler;
}

// ── Tick — fire events whose time falls within the frame ──────

void EventSystem::Tick(float /*deltaTimeMs*/)
{
	if (!enabled) return;
	if (!trackedSound) return;
	if (!ma_sound_is_playing(trackedSound)) return;

	float cursor = GetPlaybackTime();

	// Detect loop / seek-back: if cursor jumped backward, reset all fired flags
	if (cursor < lastCursor - 0.5f)
		Reset();

	for (auto& e : events)
	{
		if (e.fired) continue;

		// Event fires if the cursor has passed it (within tolerance)
		if (cursor >= e.time - tolerance && cursor <= e.time + tolerance)
		{
			e.fired = true;
			auto it = handlers.find(e.type);
			if (it != handlers.end())
				it->second(e);
		}
		else if (cursor > e.time + tolerance)
		{
			// Missed it (frame skip) — still mark as fired to avoid late trigger
			e.fired = true;
		}
	}

	lastCursor = cursor;
}

// ── Reset ─────────────────────────────────────────────────────

void EventSystem::Reset()
{
	for (auto& e : events)
		e.fired = false;
}

// ── Event management ──────────────────────────────────────────

void EventSystem::AddEvent(const TimeEvent& e)
{
	events.push_back(e);
	SortEvents();
}

void EventSystem::RemoveEvent(int index)
{
	if (index >= 0 && index < (int)events.size())
		events.erase(events.begin() + index);
	if (selectedEvent >= (int)events.size())
		selectedEvent = (int)events.size() - 1;
}

void EventSystem::SortEvents()
{
	std::sort(events.begin(), events.end(),
		[](const TimeEvent& a, const TimeEvent& b) { return a.time < b.time; });
}

// ── Save / Load (simple binary format) ────────────────────────
//
// Format:
//   4 bytes: "EVTS" magic
//   4 bytes: uint32 event count
//   Per event:
//     4 bytes: float time
//     4 bytes: uint32 type string length
//     N bytes: type string (no null terminator)
//     4 bytes: float param1
//     4 bytes: float param2
//     4 bytes: float param3
//     4 bytes: uint32 strParam length
//     N bytes: strParam string

bool EventSystem::Save(const char* path) const
{
	FILE* f = fopen(path, "wb");
	if (!f) return false;

	// Magic
	fwrite("EVTS", 1, 4, f);

	// Count
	uint32_t count = (uint32_t)events.size();
	fwrite(&count, sizeof(uint32_t), 1, f);

	for (const auto& e : events)
	{
		fwrite(&e.time, sizeof(float), 1, f);

		uint32_t typeLen = (uint32_t)e.type.size();
		fwrite(&typeLen, sizeof(uint32_t), 1, f);
		fwrite(e.type.data(), 1, typeLen, f);

		fwrite(&e.param1, sizeof(float), 1, f);
		fwrite(&e.param2, sizeof(float), 1, f);
		fwrite(&e.param3, sizeof(float), 1, f);

		uint32_t strLen = (uint32_t)e.strParam.size();
		fwrite(&strLen, sizeof(uint32_t), 1, f);
		fwrite(e.strParam.data(), 1, strLen, f);
	}

	fclose(f);
	printf("[EventSystem] Saved %d events to %s\n", count, path);
	return true;
}

bool EventSystem::Load(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) return false;

	char magic[4];
	fread(magic, 1, 4, f);
	if (memcmp(magic, "EVTS", 4) != 0)
	{
		fclose(f);
		printf("[EventSystem] Invalid file: %s\n", path);
		return false;
	}

	uint32_t count = 0;
	fread(&count, sizeof(uint32_t), 1, f);

	events.clear();
	events.reserve(count);

	for (uint32_t i = 0; i < count; i++)
	{
		TimeEvent e;
		fread(&e.time, sizeof(float), 1, f);

		uint32_t typeLen = 0;
		fread(&typeLen, sizeof(uint32_t), 1, f);
		e.type.resize(typeLen);
		fread(e.type.data(), 1, typeLen, f);

		fread(&e.param1, sizeof(float), 1, f);
		fread(&e.param2, sizeof(float), 1, f);
		fread(&e.param3, sizeof(float), 1, f);

		uint32_t strLen = 0;
		fread(&strLen, sizeof(uint32_t), 1, f);
		e.strParam.resize(strLen);
		fread(e.strParam.data(), 1, strLen, f);

		events.push_back(e);
	}

	fclose(f);
	printf("[EventSystem] Loaded %d events from %s\n", count, path);
	return true;
}

void EventSystem::Pause()
{
	if (!trackedSound) return;
	if (ma_sound_is_playing(trackedSound))
		ma_sound_stop(trackedSound);
	else
		ma_sound_start(trackedSound);
}

void EventSystem::SetSpeed(float speed)
{
	playbackSpeed = speed;
	if (trackedSound)
		ma_sound_set_pitch(trackedSound, speed);
}

void EventSystem::SeekTo(float seconds)
{
	if (!trackedSound) return;
	ma_uint32 sampleRate = ma_engine_get_sample_rate(ma_sound_get_engine(trackedSound));
	ma_uint64 frame = (ma_uint64)(seconds * sampleRate);
	ma_sound_seek_to_pcm_frame(trackedSound, frame);
	Reset();  // reset fired flags since we jumped
	lastCursor = seconds;
}

// ── ImGui Editor ──────────────────────────────────────────────
void EventSystem::UI(std::function<void()> resetAccumulator)
{
	if (!ImGui::CollapsingHeader("Event Timeline"))
		return;

	ImGui::Checkbox("Enabled", &enabled);

	float trackLength = cachedTrackLength;
	float cursor = GetPlaybackTime();
	bool playing = IsPlaying();

	// ── Transport controls ────────────────────────────────────
	ImGui::Text("Playback: %.2fs / %.1fs", cursor, trackLength);
	ImGui::SameLine();
	ImGui::TextDisabled("| %d events", (int)events.size());

	if (ImGui::Button(playing ? "Pause" : "Play", ImVec2(60, 0)))
		Pause();

	ImGui::SameLine();

	ImGui::SetNextItemWidth(150);
	if (ImGui::SliderFloat("Speed", &playbackSpeed, 0.0f, 2.0f, "%.2fx"))
		SetSpeed(playbackSpeed);

	ImGui::SameLine();
	if (ImGui::Button("1x", ImVec2(25, 0))) SetSpeed(1.0f);
	ImGui::SameLine();
	if (ImGui::Button(".5x", ImVec2(30, 0))) SetSpeed(0.5f);
	ImGui::SameLine();
	if (ImGui::Button(".25x", ImVec2(35, 0))) SetSpeed(0.25f);

	float seekPos = cursor;
	ImGui::SetNextItemWidth(-1);
	if (ImGui::SliderFloat("##seek", &seekPos, 0.0f, trackLength, "Seek: %.2fs"))
		SeekTo(seekPos);

	ImGui::Spacing();

	// ── Timeline visual ───────────────────────────────────────
	ImGui::SliderFloat("Zoom", &zoomLevel, 20.0f, 500.0f, "%.0f px/s");

	float timelineWidth = trackLength * zoomLevel;
	float availWidth = ImGui::GetContentRegionAvail().x;

	ImGui::BeginChild("##timeline", ImVec2(0, 80), true, ImGuiWindowFlags_HorizontalScrollbar);

	if (playing)
	{
		float cursorPx = cursor * zoomLevel;
		float scrollX = ImGui::GetScrollX();
		if (cursorPx < scrollX || cursorPx > scrollX + availWidth - 40)
			ImGui::SetScrollX(cursorPx - availWidth * 0.3f);
	}

	ImDrawList* draw = ImGui::GetWindowDrawList();
	ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	float canvasH = 60.0f;

	draw->AddRectFilled(canvasPos, ImVec2(canvasPos.x + timelineWidth, canvasPos.y + canvasH),
		IM_COL32(30, 30, 30, 255));

	for (float t = 0; t < trackLength; t += 1.0f)
	{
		float x = canvasPos.x + t * zoomLevel;
		bool isMajor = (int)t % 5 == 0;
		draw->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + (isMajor ? 15.0f : 8.0f)),
			IM_COL32(80, 80, 80, 255));
		if (isMajor)
		{
			char label[16];
			snprintf(label, sizeof(label), "%.0fs", t);
			draw->AddText(ImVec2(x + 2, canvasPos.y + 1), IM_COL32(120, 120, 120, 255), label);
		}
	}

	for (int i = 0; i < (int)events.size(); i++)
	{
		const auto& e = events[i];
		float x = canvasPos.x + e.time * zoomLevel;
		float y = canvasPos.y + 18.0f;

		ImU32 color = e.fired ? IM_COL32(60, 60, 60, 200) : IM_COL32(100, 200, 100, 255);
		if (i == selectedEvent)
			color = IM_COL32(255, 200, 50, 255);

		draw->AddRectFilled(ImVec2(x - 2, y), ImVec2(x + 2, y + canvasH - 20), color);
		draw->AddText(ImVec2(x + 4, y), IM_COL32(200, 200, 200, 200), e.type.c_str());
	}

	{
		float cx = canvasPos.x + cursor * zoomLevel;
		draw->AddLine(ImVec2(cx, canvasPos.y), ImVec2(cx, canvasPos.y + canvasH),
			IM_COL32(255, 50, 50, 255), 2.0f);
	}

	ImGui::InvisibleButton("##timelineBtn", ImVec2(timelineWidth, canvasH));
	if (ImGui::IsItemClicked(0))
	{
		float mx = ImGui::GetMousePos().x - canvasPos.x;
		float clickTime = mx / zoomLevel;

		int closest = -1;
		float closestDist = 999.0f;
		for (int i = 0; i < (int)events.size(); i++)
		{
			float dist = fabsf(events[i].time - clickTime);
			if (dist < 0.3f && dist < closestDist)
			{
				closest = i;
				closestDist = dist;
			}
		}
		selectedEvent = closest;
	}

	ImGui::EndChild();

	ImGui::Spacing();

	// ── Add / Remove buttons ──────────────────────────────────
	float halfW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

	if (ImGui::Button("Add Event at Cursor", ImVec2(halfW, 0)))
	{
		TimeEvent e;
		e.time = cursor;
		e.type = knownTypes.empty() ? "custom" : knownTypes[0];
		AddEvent(e);
		selectedEvent = -1;
		for (int i = 0; i < (int)events.size(); i++)
			if (fabsf(events[i].time - cursor) < 0.01f)
				selectedEvent = i;
	}
	ImGui::SameLine();
	{
		bool canRemove = selectedEvent >= 0 && selectedEvent < (int)events.size();
		if (!canRemove) ImGui::BeginDisabled();
		if (ImGui::Button("Remove Selected", ImVec2(halfW, 0)))
		{
			RemoveEvent(selectedEvent);
			selectedEvent = -1;
		}
		if (!canRemove) ImGui::EndDisabled();
	}

	if (ImGui::Button("Reset Fired Flags"))
		Reset();

	ImGui::Spacing();

	// ── Selected event editor ─────────────────────────────────
	if (selectedEvent >= 0 && selectedEvent < (int)events.size())
	{
		ImGui::Separator();
		ImGui::Text("Event #%d", selectedEvent);
		ImGui::Spacing();

		TimeEvent& e = events[selectedEvent];

		bool changed = false;
		changed |= ImGui::DragFloat("Time (s)", &e.time, 0.01f, 0.0f, trackLength, "%.3f");

		if (ImGui::BeginCombo("Type", e.type.c_str()))
		{
			for (const auto& t : knownTypes)
			{
				bool selected = (e.type == t);
				if (ImGui::Selectable(t.c_str(), selected))
				{
					e.type = t;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}

		changed |= ImGui::DragFloat("Param 1", &e.param1, 0.01f);
		changed |= ImGui::DragFloat("Param 2", &e.param2, 0.01f);
		changed |= ImGui::DragFloat("Param 3", &e.param3, 0.01f);

		static char strBuf[256] = {};
		if (e.strParam.size() < sizeof(strBuf))
			strncpy(strBuf, e.strParam.c_str(), sizeof(strBuf));
		if (ImGui::InputText("String Param", strBuf, sizeof(strBuf)))
		{
			e.strParam = strBuf;
			changed = true;
		}

		ImGui::TextDisabled(e.fired ? "Status: fired" : "Status: pending");

		if (changed)
		{
			SortEvents();
			for (int i = 0; i < (int)events.size(); i++)
			{
				if (fabsf(events[i].time - e.time) < 0.001f && events[i].type == e.type)
				{
					selectedEvent = i;
					break;
				}
			}
		}
	}

	ImGui::Spacing();

	// ── Event list ────────────────────────────────────────────
	if (ImGui::TreeNode("All Events"))
	{
		for (int i = 0; i < (int)events.size(); i++)
		{
			ImGui::PushID(i);
			const auto& e = events[i];
			bool isSel = (i == selectedEvent);

			ImGui::TextDisabled("%.2fs", e.time);
			ImGui::SameLine(60);

			if (isSel)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.8f, 0.2f, 1));

			ImGui::Text("[%s]", e.type.c_str());

			if (isSel)
				ImGui::PopStyleColor();

			ImGui::SameLine();
			ImGui::TextDisabled("p=(%.1f,%.1f,%.1f)", e.param1, e.param2, e.param3);

			if (!e.strParam.empty())
			{
				ImGui::SameLine();
				ImGui::TextDisabled("\"%s\"", e.strParam.c_str());
			}

			ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 30);
			if (ImGui::SmallButton("Sel"))
				selectedEvent = i;

			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	// ── Save / Load buttons ───────────────────────────────────
	ImGui::Spacing();
	ImGui::Separator();
	float thirdW = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
	if (ImGui::Button("Save", ImVec2(thirdW, 0)))
		Save("events.bin");
	ImGui::SameLine();
	if (ImGui::Button("Load", ImVec2(thirdW, 0)))
		Load("events.bin");
	ImGui::SameLine();
	if (ImGui::Button("Clear All", ImVec2(thirdW, 0)))
	{
		events.clear();
		selectedEvent = -1;
	}
}