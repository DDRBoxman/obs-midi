/*
obs-midi

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-frontend-api/obs-frontend-api.h>
#include <QtCore/QTime>
//#include <Python.h>
#include <functional>
#include <map>
#include <string>
#include <iostream>
#include "utils.h"
#include "midi-agent.h"
#include "obs-midi.h"
#include "obs-controller.h"
using namespace std;


///////////////////////
/* MIDI HOOK ROUTES */
//////////////////////

// BUTTON ACTIONS
map<string, function<void(MidiHook *hook, int midiVal)>> funcMap = {
	
	{"Set Current Scene", [](MidiHook* hook, int midiVal) { OBSController::SetCurrentScene(hook->param1.c_str()); }},
	{"Set Preview Scene", [](MidiHook* hook, int midiVal) { OBSController::SetPreviewScene(hook->param1.c_str()); }},
	{"Set Current Scene Collection", [](MidiHook* hook, int midiVal) { OBSController::SetCurrentSceneCollection(QString::fromStdString(hook->param1)); }},
	{"Reset Scene Item", [](MidiHook* hook, int midiVal) { OBSController::ResetSceneItem(hook->param1.c_str(), hook->param2.c_str()); }},

	{"TransitionToProgram", [](MidiHook* hook, int midiVal) {
		if (QString::fromStdString(hook->param1).isEmpty()){
			OBSController::TransitionToProgram();
		}
		else if (Utils::is_number(hook->param2)) {
			OBSController::TransitionToProgram(QString::fromStdString(hook->param1), stoi(hook->param2));
		}
		else {
			OBSController::TransitionToProgram(QString::fromStdString(hook->param1));
		}
	}},
	{"Set Current Transition", [](MidiHook* hook, int midiVal) { OBSController::SetCurrentTransition(QString::fromStdString(hook->param1)); }},
	{"SetTransitionDuration", [](MidiHook* hook, int midiVal) {
		if (Utils::is_number(hook->param1)) {
			OBSController::SetTransitionDuration(stoi(hook->param1));
		}
		else {
			OBSController::SetTransitionDuration(midiVal);
		}
	}},

	{"Toggle Mute", [](MidiHook* hook, int midiVal) { OBSController::ToggleMute(QString::fromStdString(hook->param1)); }},
	{"Set Mute", [](MidiHook* hook, int midiVal) { OBSController::SetMute(QString::fromStdString(hook->param1), midiVal); }},

	{"Start Stop Streaming", [](MidiHook* hook, int midiVal) { OBSController::StartStopStreaming(); }},
	{"Start Streaming", [](MidiHook* hook, int midiVal) { OBSController::StartStreaming(); }},
	{"Stop Streaming", [](MidiHook* hook, int midiVal) { OBSController::StopStreaming(); }},

	{"Start Stop Recording", [](MidiHook* hook, int midiVal) { OBSController::StartStopRecording(); }},
	{"Start Recording", [](MidiHook* hook, int midiVal) { OBSController::StartRecording(); }},
	{"Stop Recording", [](MidiHook* hook, int midiVal) { OBSController::StopRecording(); }},
	{"Pause Recording", [](MidiHook* hook, int midiVal) { OBSController::PauseRecording(); }},
	{"Resume Recording", [](MidiHook* hook, int midiVal) { OBSController::ResumeRecording(); }},

	{"Start Stop Replay Buffer", [](MidiHook* hook, int midiVal) { OBSController::StartStopReplayBuffer(); }},
	{"Start Replay Buffer", [](MidiHook* hook, int midiVal) { OBSController::StartReplayBuffer(); }},
	{"Stop Replay Buffer", [](MidiHook* hook, int midiVal) { OBSController::StopReplayBuffer(); }},
	{"Save Replay Buffer", [](MidiHook* hook, int midiVal) { OBSController::SaveReplayBuffer(); }},

	{"Set Current Profile", [](MidiHook* hook, int midiVal) { OBSController::SetCurrentProfile(QString::fromStdString(hook->param1)); }},
	{"Set Text GDIPlus Text", [](MidiHook* hook, int midiVal) { OBSController::SetTextGDIPlusText(); }},
	{"Set Browser Source URL", [](MidiHook* hook, int midiVal) { OBSController::SetBrowserSourceURL(); }},
	{"Reload Browser Source", [](MidiHook* hook, int midiVal) { OBSController::ReloadBrowserSource(); }},
	{"Take Source Screenshot", [](MidiHook* hook, int midiVal) { OBSController::TakeSourceScreenshot(QString::fromStdString(hook->param1)); }},
	{"Enable Source Filter", [](MidiHook* hook, int midiVal) { OBSController::EnableSourceFilter(); }},
	{"Disable Source Filter", [](MidiHook* hook, int midiVal) { OBSController::DisableSourceFilter(); }},
	{"Toggle Source Filter", [](MidiHook* hook, int midiVal) { OBSController::ToggleSourceFilter(); }},

	// CC ACTIONS
	{"Set Volume",
	 [](MidiHook *hook, int midiVal) {  OBSController::SetVolume(QString::fromStdString(hook->param1), pow(Utils::mapper(midiVal), 3.0));
	 }},
	{"Set Sync Offset", [](MidiHook* hook, int midiVal) { OBSController::SetSyncOffset(QString::fromStdString(hook->param1), (int64_t) midiVal); }},
	{"Set Source Position", [](MidiHook* hook, int midiVal) { OBSController::SetSourcePosition(); }},
	{"Set Source Rotation", [](MidiHook* hook, int midiVal) { OBSController::SetSourceRotation(); }},
	{"Set Source Scale", [](MidiHook* hook, int midiVal) { OBSController::SetSourceScale(); }},
	{"Set Gain Filter", [](MidiHook* hook, int midiVal) { OBSController::SetGainFilter(); }},
	{"Set Opacity", [](MidiHook* hook, int midiVal) { OBSController::SetOpacity(); }}
};



////////////////
// MIDI AGENT //
////////////////
MidiAgent::MidiAgent()
{
	name = "Midi Device (uninit)";
	midiin = new rtmidi::midi_in();
	midiin->set_callback([this](const auto &message) { HandleInput(message, this); });
}

MidiAgent::~MidiAgent()
{
	ClosePort();
	delete midiin;
}

/* Loads information from OBS data. (recalled from Config)
 * This will not enable the MidiAgent or open the port. (and shouldn't)
 */
void MidiAgent::Load(obs_data_t * data)
{
	name = obs_data_get_string(data, "name");
	enabled = obs_data_get_bool(data, "enabled");
	obs_data_array_t* hooksData = obs_data_get_array(data, "hooks");
	size_t hooksCount = obs_data_array_count(hooksData);
	for (size_t i = 0; i < hooksCount; i++)
	{
		obs_data_t* hookData = obs_data_array_item(hooksData, i);
		MidiHook* mh = new MidiHook(
			obs_data_get_string(hookData, "type"),
			obs_data_get_int(hookData, "index"),
			obs_data_get_string(hookData, "command"),
			obs_data_get_string(hookData, "param1"),
			obs_data_get_string(hookData, "param2"),
			obs_data_get_string(hookData, "param3"),
			obs_data_get_string(hookData, "action")
		);
		AddMidiHook(mh);
	}

}
void MidiAgent::SendMessage(std::string name, std::string mType, int mIndex) {

	emit this->SendNewUnknownMessage(QString::fromStdString(name), QString::fromStdString(mType), mIndex);
	
}

	/* Will open the port and enable this MidiAgent
*/
void MidiAgent::OpenPort(int port)
{
	midiin->open_port(port);
	name = midiin->get_port_name(port);
	enabled = true;
	connected = true;
	blog(LOG_INFO, "MIDI device connected: [%d] %s", port, name.c_str());
}

/* Will close the port and disable this MidiAgent
*/
void MidiAgent::ClosePort()
{
	midiin->close_port();
	enabled = false;
	connected = false;
}


string MidiAgent::GetName() { return name; }
int MidiAgent::GetPort() { return port; }
bool MidiAgent::isEnabled() { return enabled; }
bool MidiAgent::isConnected() { return connected; }


/* Midi input callback.
 * Extend input handling functionality here.
 * For OBS command triggers, edit the funcMap instead.
 */
void MidiAgent::HandleInput(const rtmidi::message &message, void *userData)
{       
	MidiAgent *self = static_cast<MidiAgent *>(userData);
	if (self->enabled == false || self->connected == false){ return; }

	/*************Get Message parts***********/
	auto mType = Utils::mtype_to_string(message.get_message_type());
	int mchannel = message.get_channel();
	auto byt = message.bytes;
	auto norc = Utils::get_midi_note_or_control(message);
	auto value = Utils::get_midi_value(message);
	
	/***** Send Messages to emit function *****/
	self->SendMessage(self->name, mType, norc);

	
	/** check if hook exists for this note or cc index and launch it **/
	//Eventually add channel to this check.
	
	for (unsigned i = 0; i < self->midiHooks.size(); i++) {
		if (self->midiHooks.at(i)->type == mType && self->midiHooks.at(i)->index == norc) {
			self->TriggerInputCommand(self->midiHooks.at(i), value);
			
		}
	}
	
	
}

/* Triggers funcMap function. Called from HandleInput callback
*/
void MidiAgent::TriggerInputCommand(MidiHook* hook, int midiVal)
{
	blog(LOG_INFO, "Triggered: %d [%d] %s %s", hook->index, midiVal, hook->command.c_str(), hook->param1.c_str());
	 funcMap[hook->command](hook, midiVal);
}		

/* Get the midi hooks for this device
*/
vector<MidiHook*> MidiAgent::GetMidiHooks()
{
	return midiHooks;
}

/* Add a new MidiHook
*/
void MidiAgent::AddMidiHook(MidiHook* hook)
{
	midiHooks.push_back(hook);
}

/* Remove a MidiHook
*/
void MidiAgent::RemoveMidiHook(MidiHook* hook)
{
	auto it = std::find(midiHooks.begin(), midiHooks.end(), hook);
	if (it != midiHooks.end()) {
		midiHooks.erase(it);
	}
}

/* Clears all the MidiHooks for this device.
*/
void MidiAgent::ClearMidiHooks()
{
	midiHooks.clear();
}

/* Get this MidiAgent state as OBS Data. (includes midi hooks)
* This is needed to Serialize the state in the config.
* https://obsproject.com/docs/reference-settings.html
*/
obs_data_t* MidiAgent::GetData() {
	obs_data_t* data = obs_data_create();
	obs_data_set_string(data, "name", name.c_str());
	obs_data_set_bool(data, "enabled", enabled);

	obs_data_array_t* arrayData = obs_data_array_create();
	for (int i = 0; i < midiHooks.size(); i++)
	{
		obs_data_t* hookData = midiHooks.at(i)->GetData();
		obs_data_array_push_back(arrayData, hookData);
	}
	obs_data_set_array(data, "hooks", arrayData);
	return data;
}
