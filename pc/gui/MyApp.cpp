#include "MyApp.h"
#include "MyFrame.h"

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    MyFrame *frame = new MyFrame("FlexFloppy", wxPoint(50, 50), wxSize(450, 340));
    frame->Show(true);
    return true;
}
