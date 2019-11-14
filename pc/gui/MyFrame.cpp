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
    EVT_COMMAND(wxID_ANY, wxEVT_USBTRACKSUCCESS, MyFrame::OnTrackSuccess)
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

    // 2 Static boxes (Side A+B)
    wxStaticBox *staticBoxSide[2];
    staticBoxSide[0] = new wxStaticBox(this, wxID_ANY, "Side A");
    staticBoxSide[1] = new wxStaticBox(this, wxID_ANY, "Side B");

    // Horizontal root sizer
    wxBoxSizer *rootSizer = new wxBoxSizer(wxHORIZONTAL);
    rootSizer->AddSpacer(10);
    rootSizer->Add(staticBoxSide[0]);
    rootSizer->AddSpacer(10);
    rootSizer->Add(staticBoxSide[1]);

    // Track grid sizers
    wxGridSizer *gridSizerSide[2];
    gridSizerSide[0] = new wxGridSizer(8,10,0,0);
    gridSizerSide[1] = new wxGridSizer(8,10,0,0);
    staticBoxSide[0]->SetSizer(gridSizerSide[0]);
    staticBoxSide[1]->SetSizer(gridSizerSide[1]);

    // 160 text boxes
    for(int side = 0; side < 2; side++) {
        for(int n=0; n<80; n++) {
            staticTextTrack[side][n] = new wxStaticText(staticBoxSide[side], wxID_ANY, L"\u2717");
            gridSizerSide[side]->Add(staticTextTrack[side][n],
                wxSizerFlags().Align(wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL));
        }
    }
    this->SetSizer(rootSizer);
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
    SetStatusText(string);
    wxLogMessage(string);
}
void MyFrame::OnTrackSuccess(wxCommandEvent& event)
{
    int track = event.GetInt();
    staticTextTrack[track%2][track/2]->SetLabel(L"\u2713");
}
