#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <string>

class MyFrame: public wxFrame
{
public:
    MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
private:
    wxStaticText *staticTextTrack[2][80];
    void OnExit(wxCommandEvent& event);
		void StartRead(wxCommandEvent& event);
		void OnUSBStatus(wxCommandEvent& event);
		void OnUSBAlert(wxCommandEvent& event);
		void OnTrackSuccess(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();
};
