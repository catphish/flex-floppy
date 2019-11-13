#include "MyFrame.h"
#include "USBThread.h"

enum {
    ID_READ = 1
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU(wxID_EXIT, MyFrame::OnExit)
    EVT_MENU(ID_READ,   MyFrame::StartRead)
    EVT_COMMAND(wxID_ANY, wxEVT_USBALERT, MyFrame::OnUSBAlert)
    EVT_COMMAND(wxID_ANY, wxEVT_USBSTATUS, MyFrame::OnUSBStatus)
wxEND_EVENT_TABLE()

MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
        : wxFrame(NULL, wxID_ANY, title, pos, size)
{
    wxMenu *menuFile = new wxMenu;
    menuFile->Append(ID_READ, "&Read Disk");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);
    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append( menuFile, "&File" );
    SetMenuBar( menuBar );
    CreateStatusBar();
    SetStatusText( "FlexFloppy" );
    libusb_init(NULL);
}
void MyFrame::OnExit(wxCommandEvent& event)
{
    Close( true );
}
void MyFrame::StartRead(wxCommandEvent& event)
{
    USBThread *thread = new USBThread(this);
    thread->action = &USBThread::ReadData;
    thread->Create();
    thread->Run();
}
void MyFrame::OnUSBStatus(wxCommandEvent& event)
{
    wxString string = event.GetString();
    SetStatusText(string);
}
void MyFrame::OnUSBAlert(wxCommandEvent& event)
{
    wxString string = event.GetString();
    wxLogMessage(string);
}
