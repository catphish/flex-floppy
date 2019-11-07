#include <libusb.h>
#include "MyThread.h"

DEFINE_EVENT_TYPE(wxEVT_MYTHREAD)

MyThread::MyThread(wxEvtHandler* pParent) : wxThread(wxTHREAD_DETACHED), m_pParent(pParent){}

int event_thread_run = 1;

int hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev,
                     libusb_hotplug_event event, void *user_data) {
  static libusb_device_handle *handle = NULL;
  struct libusb_device_descriptor desc;
  int rc;
  (void)libusb_get_device_descriptor(dev, &desc);
  if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event) {
    rc = libusb_open(dev, &handle);
	  printf("USB Inserted!\n");
    if (LIBUSB_SUCCESS != rc) {
      printf("Could not open USB device\n");
    }
  } else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
    if (handle) {
		  printf("USB Ejected!\n");
      libusb_close(handle);
      handle = NULL;
    }
  } else {
    printf("Unhandled event %d\n", event);
  }
  return 0;
}

void* MyThread::Entry()
{

  libusb_hotplug_callback_handle handle;
  int rc;
  libusb_init(NULL);
  rc = libusb_hotplug_register_callback(NULL, (libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                        LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT), (libusb_hotplug_flag)0, 0x1209, 0x0001,
                                        LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, NULL,
                                        &handle);
  if (LIBUSB_SUCCESS != rc) {
    printf("Error creating a hotplug callback\n");
    libusb_exit(NULL);
    return 0;
  }
  while (event_thread_run)
	  libusb_handle_events_completed(NULL, NULL);

  libusb_hotplug_deregister_callback(NULL, handle);
  libusb_exit(NULL);
  return 0;


  // wxCommandEvent evt(wxEVT_MYTHREAD, GetId());
  // //can be used to set some identifier for the data
  // evt.SetInt(0x10); 

  // wxPostEvent(m_pParent, evt);
  // return 0;
}
