#include "USBThread.h"

wxDEFINE_EVENT(wxEVT_USBALERT, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_USBSTATUS, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_USBTRACKSUCCESS, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_USBTRACKFAILURE, wxCommandEvent);

USBThread::USBThread(wxEvtHandler* pParent) : wxThread(wxTHREAD_DETACHED), m_pParent(pParent){}

static wxMutex s_USBLock;

void* USBThread::Entry()
{
	if(s_USBLock.TryLock() == wxMUTEX_BUSY) {
		Alert("USB operaton already in progress");
		return 0;
	}
	Status("USB thread started.");
  if(libusb_init(NULL)) {
  	Alert("USB initialization failed.");
  } else {
  	handle = libusb_open_device_with_vid_pid(NULL, 0x1209, 0x0001);
  	if(handle) {
			Status("USB device found.");
  		(this->*action)();
			libusb_close(handle);
  	} else {
			Alert("USB device not found.");
  	}
  }
  s_USBLock.Unlock();
  return 0;
}

void USBThread::ReadData()
{
	// Enable drive
	Status("Beginning read operation.");
	libusb_control_transfer(handle, 0x41, 0x01, 0, 0, NULL, 0, 2000);

	// Zero head
	Status("Zeroing head.");
	libusb_control_transfer(handle, 0x41, 0x11, 0, 0, NULL, 0, 2000);

	// Main read loop
	uint8_t * buffer_d = (uint8_t*)malloc(1024 * 1024);
	uint8_t * buffer_i = (uint8_t*)malloc(1024);
	uint8_t * buffer_s = (uint8_t*)malloc(1024);
	int transferred;

	for(int track = 0; track < 10; track++) {
		char message[64];
		sprintf(message, "Reading track %i.", track); Status(message);
		
		libusb_control_transfer(handle, 0x41, 0x12, track, 0, NULL, 0, 2000);
		libusb_control_transfer(handle, 0x41, 0x21, 2, 0, NULL, 0, 2000);
		
		libusb_bulk_transfer(handle, 0x82, buffer_d, 1024 * 1024, &transferred, 2000);
		libusb_bulk_transfer(handle, 0x82, buffer_i, 1024, &transferred, 2000);
		libusb_bulk_transfer(handle, 0x82, buffer_s, 1024, &transferred, 2000);
		printf("STATUS: %i %i\n", buffer_s[0], buffer_s[1]);
		TrackSuccess(track);
		if(aborted) break;
	}
	free(buffer_d); free(buffer_i); free(buffer_s);
	
	// Disable Drive
	Status("Disabling drve.");
	libusb_control_transfer(handle, 0x41, 0x02, 0, 0, NULL, 0, 2000);

	Status("USB operation complete.");
}

void USBThread::Alert(const char* message)
{
		printf("%s\n", message);
	  wxCommandEvent evt(wxEVT_USBALERT);
	  evt.SetString(message);
	  wxPostEvent(m_pParent, evt);
}
void USBThread::Status(const char* message)
{
		printf("%s\n", message);
	  wxCommandEvent evt(wxEVT_USBSTATUS);
	  evt.SetString(message);
	  wxPostEvent(m_pParent, evt);
}
void USBThread::TrackSuccess(int track)
{
	  wxCommandEvent evt(wxEVT_USBTRACKSUCCESS);
	  evt.SetInt(track);
	  wxPostEvent(m_pParent, evt);
}
