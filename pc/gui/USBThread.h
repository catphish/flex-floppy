#ifndef USBTHREAD_H
#define USBTHREAD_H

#include <string>
#include <libusb.h>
#include <wx/thread.h>
#include <wx/event.h>

int hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev,
                     libusb_hotplug_event event, void* USBThread);

BEGIN_DECLARE_EVENT_TYPES()
	wxDECLARE_EVENT(wxEVT_USBALERT, wxCommandEvent);
  wxDECLARE_EVENT(wxEVT_USBSTATUS, wxCommandEvent);
  wxDECLARE_EVENT(wxEVT_USBTRACKSUCCESS, wxCommandEvent);
  wxDECLARE_EVENT(wxEVT_USBTRACKFAILURE, wxCommandEvent);
END_DECLARE_EVENT_TYPES()

class USBThread : public wxThread
{
  public:
    USBThread(wxEvtHandler* pParent);
    void (USBThread::* action)();
		void ReadData();
  protected:
		void* Entry();
    wxEvtHandler* m_pParent;
  private:
  	void Status(const char* message);
  	void Alert(const char* message);
    void TrackSuccess(int track);
    bool aborted;
  	libusb_device_handle* handle;
};
#endif
