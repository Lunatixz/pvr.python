/*
 *  pvr.python - A PVR client for Kodi using Python
 *  Copyright © 2016 RunasSudo (Yingtong Li)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <Python.h>

#include "client.h"
#include "xbmc_pvr_dll.h"
#include "PVRDemoData.h"
#include <p8-platform/util/util.h>

using namespace std;
using namespace ADDON;

#ifdef TARGET_WINDOWS
#define snprintf _snprintf
#endif

bool m_bCreated  = false;
ADDON_STATUS  m_CurStatus = ADDON_STATUS_UNKNOWN;
PVRDemoData *m_data = NULL;
bool m_bIsPlaying  = false;
PVRDemoChannel m_currentChannel;

/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
std::string g_strUserPath = "";
std::string g_strClientPath = "";

CHelper_libXBMC_addon *XBMC = NULL;
CHelper_libXBMC_pvr *PVR = NULL;

PyThreadState* pyState;
PyObject* pyModule;

extern "C" {

// BEGIN PYTHON BRIDGE FUNCTIONS

static PyObject* bridge_log(PyObject *self, PyObject *args)
{
	const char *s;
	if (!PyArg_ParseTuple(args, "s", &s)) {
		PyErr_SetString(PyExc_TypeError, "parameter must be a string");
		return NULL;
	}
	
	XBMC->Log(LOG_DEBUG, "%s - %s", __FUNCTION__, s);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef bridgeMethods[] = {
	{"log", bridge_log, METH_VARARGS, ""},
	{NULL, NULL, 0, NULL}
};

// END PYTHON BRIDGE FUNCTIONS

void ADDON_ReadSettings(void)
{
	//STUB
}

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
	if (!hdl || !props)
		return ADDON_STATUS_UNKNOWN;
	
	PVR_PROPERTIES* pvrprops = (PVR_PROPERTIES*)props;
	
	XBMC = new CHelper_libXBMC_addon;
	if (!XBMC->RegisterMe(hdl))
	{
		SAFE_DELETE(XBMC);
		return ADDON_STATUS_PERMANENT_FAILURE;
	}
	
	PVR = new CHelper_libXBMC_pvr;
	if (!PVR->RegisterMe(hdl))
	{
		SAFE_DELETE(PVR);
		SAFE_DELETE(XBMC);
		return ADDON_STATUS_PERMANENT_FAILURE;
	}
	
	XBMC->Log(LOG_DEBUG, "%s - Creating the PVR demo add-on", __FUNCTION__);
	
	PyEval_AcquireLock();
	pyState = Py_NewInterpreter();
	PyThreadState_Swap(pyState);
	
	Py_InitModule("bridge", bridgeMethods);
	
	// Setup the path
	PyObject* sysPath = PySys_GetObject((char*) "path");
	PyObject* pyClientPath = PyString_FromString(pvrprops->strClientPath);
	PyList_Append(sysPath, pyClientPath);
	Py_DECREF(pyClientPath);
	XBMC->Log(LOG_DEBUG, "%s - Added '%s' to sys.path", __FUNCTION__, pvrprops->strClientPath);
	
	// Import the module
	PyObject* pyName = PyString_FromString("pvrimpl");
	pyModule = PyImport_Import(pyName);
	Py_DECREF(pyName);
	
	if (pyModule == NULL) {
		XBMC->Log(LOG_DEBUG, "%s - Failed to import Python PVR implementation module 'pvrimpl'", __FUNCTION__);
		SAFE_DELETE(PVR);
		SAFE_DELETE(XBMC);
		return ADDON_STATUS_PERMANENT_FAILURE;
	}
	
	XBMC->Log(LOG_DEBUG, "%s - Handing over to Python", __FUNCTION__);
	
	// Call the function
	PyObject* pyFunc = PyObject_GetAttrString(pyModule, "ADDON_Create");
	PyObject* pyArgs = PyTuple_New(0);
	PyObject* pyReturnValue = PyObject_CallObject(pyFunc, pyArgs);
	long returnValue = PyInt_AsLong(pyReturnValue);
	Py_DECREF(pyReturnValue);
	Py_DECREF(pyArgs);
	Py_DECREF(pyFunc);
	
	Py_EndInterpreter(pyState);
	PyThreadState_Swap(NULL);
	PyEval_ReleaseLock();
	XBMC->Log(LOG_DEBUG, "%s - Finalised", __FUNCTION__);
	
	m_CurStatus = ADDON_STATUS_UNKNOWN;
	g_strUserPath = pvrprops->strUserPath;
	g_strClientPath = pvrprops->strClientPath;
	
	ADDON_ReadSettings();
	
	m_data = new PVRDemoData;
	m_CurStatus = ADDON_STATUS_OK;
	m_bCreated = true;
	
	// Process the return value
	// Enums take on their integer indexes as value
	return ((ADDON_STATUS) returnValue);
}

ADDON_STATUS ADDON_GetStatus()
{
	return m_CurStatus;
}

void ADDON_Destroy()
{
	delete m_data;
	m_bCreated = false;
	m_CurStatus = ADDON_STATUS_UNKNOWN;
}

bool ADDON_HasSettings()
{
	return true;
}

unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
	return 0;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
	return ADDON_STATUS_OK;
}

void ADDON_Stop()
{
}

void ADDON_FreeSettings()
{
}

/***********************************************************
 * PVR Client AddOn specific public library functions
 ***********************************************************/

void OnSystemSleep()
{
}

void OnSystemWake()
{
}

void OnPowerSavingActivated()
{
}

void OnPowerSavingDeactivated()
{
}

const char* GetPVRAPIVersion(void)
{
	static const char *strApiVersion = XBMC_PVR_API_VERSION;
	return strApiVersion;
}

const char* GetMininumPVRAPIVersion(void)
{
	static const char *strMinApiVersion = XBMC_PVR_MIN_API_VERSION;
	return strMinApiVersion;
}

const char* GetGUIAPIVersion(void)
{
	return ""; // GUI API not used
}

const char* GetMininumGUIAPIVersion(void)
{
	return ""; // GUI API not used
}

PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
	pCapabilities->bSupportsEPG = true;
	pCapabilities->bSupportsTV = true;
	pCapabilities->bSupportsRadio = true;
	pCapabilities->bSupportsChannelGroups = true;
	pCapabilities->bSupportsRecordings = true;
	pCapabilities->bSupportsRecordingsUndelete = true;
	pCapabilities->bSupportsTimers = true;
	
	return PVR_ERROR_NO_ERROR;
}

const char *GetBackendName(void)
{
	static const char *strBackendName = "pulse-eight demo pvr add-on";
	return strBackendName;
}

const char *GetBackendVersion(void)
{
	static CStdString strBackendVersion = "0.1";
	return strBackendVersion.c_str();
}

const char *GetConnectionString(void)
{
	static CStdString strConnectionString = "connected";
	return strConnectionString.c_str();
}

const char *GetBackendHostname(void)
{
	return "";
}

PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{
	*iTotal = 1024 * 1024 * 1024;
	*iUsed = 0;
	return PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
	if (m_data)
		return m_data->GetEPGForChannel(handle, channel, iStart, iEnd);
	
	return PVR_ERROR_SERVER_ERROR;
}

int GetChannelsAmount(void)
{
	if (m_data)
		return m_data->GetChannelsAmount();
	
	return -1;
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
	if (m_data)
		return m_data->GetChannels(handle, bRadio);
	
	return PVR_ERROR_SERVER_ERROR;
}

bool OpenLiveStream(const PVR_CHANNEL &channel)
{
	if (m_data)
	{
		CloseLiveStream();
		
		if (m_data->GetChannel(channel, m_currentChannel))
		{
			m_bIsPlaying = true;
			return true;
		}
	}
	
	return false;
}

void CloseLiveStream(void)
{
	m_bIsPlaying = false;
}

bool SwitchChannel(const PVR_CHANNEL &channel)
{
	CloseLiveStream();
	
	return OpenLiveStream(channel);
}

PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* pProperties)
{
	return PVR_ERROR_NOT_IMPLEMENTED;
}

int GetChannelGroupsAmount(void)
{
	if (m_data)
		return m_data->GetChannelGroupsAmount();
	
	return -1;
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
	if (m_data)
		return m_data->GetChannelGroups(handle, bRadio);
	
	return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
	if (m_data)
		return m_data->GetChannelGroupMembers(handle, group);
	
	return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
	snprintf(signalStatus.strAdapterName, sizeof(signalStatus.strAdapterName), "pvr demo adapter 1");
	snprintf(signalStatus.strAdapterStatus, sizeof(signalStatus.strAdapterStatus), "OK");
	
	return PVR_ERROR_NO_ERROR;
}

int GetRecordingsAmount(bool deleted)
{
	if (m_data)
		return m_data->GetRecordingsAmount(deleted);
	
	return -1;
}

PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted)
{
	if (m_data)
		return m_data->GetRecordings(handle, deleted);
	
	return PVR_ERROR_NOT_IMPLEMENTED;
}

PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *size)
{
	/* TODO: Implement this to get support for the timer features introduced with PVR API 1.9.7 */
	return PVR_ERROR_NOT_IMPLEMENTED;
}

int GetTimersAmount(void)
{
	if (m_data)
		return m_data->GetTimersAmount();
	
	return -1;
}

PVR_ERROR GetTimers(ADDON_HANDLE handle)
{
	if (m_data)
		return m_data->GetTimers(handle);
	
	/* TODO: Change implementation to get support for the timer features introduced with PVR API 1.9.7 */
	return PVR_ERROR_NOT_IMPLEMENTED;
}

/** UNUSED API FUNCTIONS */
PVR_ERROR OpenDialogChannelScan(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR CallMenuHook(const PVR_MENUHOOK &menuhook, const PVR_MENUHOOK_DATA &item) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR MoveChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
bool OpenRecordedStream(const PVR_RECORDING &recording) { return false; }
void CloseRecordedStream(void) {}
int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */) { return 0; }
long long PositionRecordedStream(void) { return -1; }
long long LengthRecordedStream(void) { return 0; }
void DemuxReset(void) {}
void DemuxFlush(void) {}
int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekLiveStream(long long iPosition, int iWhence /* = SEEK_SET */) { return -1; }
long long PositionLiveStream(void) { return -1; }
long long LengthLiveStream(void) { return -1; }
const char * GetLiveStreamURL(const PVR_CHANNEL &channel) { return ""; }
PVR_ERROR DeleteRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording) { return -1; }
PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int*) { return PVR_ERROR_NOT_IMPLEMENTED; };
PVR_ERROR AddTimer(const PVR_TIMER &timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UpdateTimer(const PVR_TIMER &timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
void DemuxAbort(void) {}
DemuxPacket* DemuxRead(void) { return NULL; }
unsigned int GetChannelSwitchDelay(void) { return 0; }
void PauseStream(bool bPaused) {}
bool CanPauseStream(void) { return false; }
bool CanSeekStream(void) { return false; }
bool SeekTime(double,bool,double*) { return false; }
void SetSpeed(int) {};
bool IsTimeshifting(void) { return false; }
bool IsRealTimeStream(void) { return true; }
time_t GetPlayingTime() { return 0; }
time_t GetBufferTimeStart() { return 0; }
time_t GetBufferTimeEnd() { return 0; }
PVR_ERROR UndeleteRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteAllRecordingsFromTrash() { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetEPGTimeFrame(int) { return PVR_ERROR_NOT_IMPLEMENTED; }
}