// DasherInterfaceBase.cpp
//
// Copyright (c) 2002 Iain Murray

#include "../Common/Common.h"

#include "DasherInterfaceBase.h"

#include "CustomColours.h"
#include "DasherViewSquare.h"
#include "ControlManager.h"
#include "DashEdit.h"
#include "DasherScreen.h"
#include "DasherView.h"
#include "DasherInput.h"
#include "DasherModel.h"
#include "EventHandler.h"
#include "Event.h"
#include "UserLog.h"
#include "DasherButtons.h"

#include <iostream>
#include <memory>
namespace {
  #include "stdio.h"
}

// Declare our global file logging object
#include "../DasherCore/FileLogger.h"
#ifdef _DEBUG
const eLogLevel g_iLogLevel   = logDEBUG;
const int       g_iLogOptions = logTimeStamp | logDateStamp | logDeleteOldFile;    
#else
const eLogLevel g_iLogLevel   = logNORMAL;
const int       g_iLogOptions = logTimeStamp | logDateStamp;
#endif
CFileLogger* g_pLogger = NULL;

using namespace Dasher;
using namespace std;

// Track memory leaks on Windows to the line that new'd the memory
#ifdef _WIN32
#ifdef _DEBUG
#define DEBUG_NEW new( _NORMAL_BLOCK, THIS_FILE, __LINE__ )
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
#endif

const string CDasherInterfaceBase::EmptyString = "";

CDasherInterfaceBase::CDasherInterfaceBase()
                  :m_Alphabet(0), m_pColours(0), m_pDasherModel(0), m_DashEditbox(0), m_DasherScreen(0),
                  m_pDasherView(0), m_pInput(0), m_AlphIO(0), m_ColourIO(0), m_pUserLog(NULL), 
                  m_pDasherButtons(NULL) {

  m_pEventHandler = new CEventHandler(this);
  strCurrentContext = ". ";

  strTrainfileBuffer = "";

  // Global logging object we can use from anywhere
  g_pLogger = new CFileLogger("dasher.log",
                              g_iLogLevel,
                              g_iLogOptions);

}

void CDasherInterfaceBase::Realize() {

  ChangeColours(GetStringParameter(SP_COLOUR_ID));
  ChangeAlphabet(GetStringParameter(SP_ALPHABET_ID));

  if(GetLongParameter(LP_ORIENTATION) == Dasher::Opts::AlphabetDefault)
    SetLongParameter(LP_REAL_ORIENTATION, GetAlphabetOrientation());
  else
    SetLongParameter(LP_REAL_ORIENTATION, GetLongParameter(LP_ORIENTATION));

  // Create the user logging object if we are suppose to.  We wait
  // until now so we have the real value of the parameter and not
  // just the default.
  int iUserLogLevel = GetLongParameter(LP_USER_LOG_LEVEL_MASK);
  if (iUserLogLevel > 0) 
    m_pUserLog = new CUserLog(m_pEventHandler, m_pSettingsStore, iUserLogLevel, m_Alphabet);  

    m_pDasherButtons = new CDasherButtons(m_pSettingsStore);
    
  // All the setup is done by now, so let the user log object know
  // that future parameter changes should be logged.
  if (m_pUserLog != NULL) 
    m_pUserLog->InitIsDone();
  
}

CDasherInterfaceBase::~CDasherInterfaceBase() {

  delete m_pDasherModel;        // The order of some of these deletions matters
  delete m_Alphabet;
  delete m_pDasherView;
  delete m_ColourIO;
  delete m_AlphIO;
  delete m_pColours;
  delete m_pDasherButtons;
  // Do NOT delete Edit box or Screen. This class did not create them.

  // When we destruct on shutdown, we'll output any detailed log file
  if (m_pUserLog != NULL)
  {
    m_pUserLog->OutputFile();
    delete m_pUserLog;
    m_pUserLog = NULL;
  }

  if (g_pLogger != NULL) {
    delete g_pLogger;
    g_pLogger = NULL;
  }

  // Must delete event handler after all CDasherComponent derived classes

  delete m_pEventHandler;
}

void CDasherInterfaceBase::ExternalEventHandler(Dasher::CEvent *pEvent) {
  
  // Obsolete (overwritten by child)

  // Pass events outside
  if(pEvent->m_iEventType == 1) {
    //HandleParameterNotification(pEvent->m_iEventType);
  }
  else if((pEvent->m_iEventType >= 2) && (pEvent->m_iEventType <= 5)) {
    if(m_DashEditbox != NULL)
      m_DashEditbox->HandleEvent(pEvent);
  }

}

void CDasherInterfaceBase::InterfaceEventHandler(Dasher::CEvent *pEvent) {

  if(pEvent->m_iEventType == 1) {
    Dasher::CParameterNotificationEvent * pEvt(static_cast < Dasher::CParameterNotificationEvent * >(pEvent));

    switch (pEvt->m_iParameter) {

    case BP_COLOUR_MODE:       // Forces us to redraw the display
      Start();
      RequestFullRedraw();
      break;
    case BP_OUTLINE_MODE:
      RequestFullRedraw();
      break;
    case LP_ORIENTATION:
      if(GetLongParameter(LP_ORIENTATION) == Dasher::Opts::AlphabetDefault)
	SetLongParameter(LP_REAL_ORIENTATION, GetAlphabetOrientation());
      else
	SetLongParameter(LP_REAL_ORIENTATION, GetLongParameter(LP_ORIENTATION));
      RequestFullRedraw();
      break;
    case SP_ALPHABET_ID:

      // I was having problems with X errors when calling a redraw
      // here - remember that this is called from the training thread,
      // which leads me to believe that pango is not thread safe, so
      // instead of actually calling a redraw we just flag for a full
      // redraw to be performed at the next timer callback. This might
      // not be a bad thing to do elsewhere too, as it will prevent
      // multiple redraws.

      ChangeAlphabet(GetStringParameter(SP_ALPHABET_ID)); 

      // FIXME - the new widgets in the preferences box are breaking this
      
      if(GetLongParameter(LP_ORIENTATION) == Dasher::Opts::AlphabetDefault)
 	SetLongParameter(LP_REAL_ORIENTATION, GetAlphabetOrientation());
      
      Start();
      RequestFullRedraw();
      break;
    case SP_COLOUR_ID:
      ChangeColours(GetStringParameter(SP_COLOUR_ID));
      RequestFullRedraw();
      break;
    case LP_LANGUAGE_MODEL_ID:
      CreateDasherModel();
      Start();
      RequestFullRedraw();
      break;
    case LP_LINE_WIDTH:
      RequestFullRedraw(); // TODO - make this accessible everywhere
      break;
    case LP_DASHER_FONTSIZE:
      // TODO - make screen a CDasherComponent child?
      RequestFullRedraw();
    case BP_MOUSEPOS_MODE:
      if(GetBoolParameter(BP_DASHER_PAUSED))
	if(GetBoolParameter(BP_MOUSEPOS_MODE))
	  SetLongParameter(LP_MOUSE_POS_BOX, 1);
	else
	  SetLongParameter(LP_MOUSE_POS_BOX, -1); 
      RequestFullRedraw();
    default:
      break;
    }
  }
  else if(pEvent->m_iEventType == 2) {
    CEditEvent *pEditEvent(static_cast < CEditEvent * >(pEvent));
    
    if(pEditEvent->m_iEditType == 1) {
      strCurrentContext += pEditEvent->m_sText;
      if( strCurrentContext.size() > 20 )
	strCurrentContext = strCurrentContext.substr( strCurrentContext.size() - 20 );

      strTrainfileBuffer += pEditEvent->m_sText;
    }
    else if(pEditEvent->m_iEditType == 2) {
      strCurrentContext = strCurrentContext.substr( 0, strCurrentContext.size() - pEditEvent->m_sText.size());

      strTrainfileBuffer = strTrainfileBuffer.substr( 0, strTrainfileBuffer.size() - pEditEvent->m_sText.size());
    }
  }
  else if(pEvent->m_iEventType == EV_CONTROL) {
    CControlEvent *pControlEvent(static_cast <CControlEvent*>(pEvent));

    switch(pControlEvent->m_iID) {
    case CControlManager::CTL_STOP:
      PauseAt(0,0);
      break;
    case CControlManager::CTL_PAUSE:
      Halt();
      break;
    }

  }
}

void CDasherInterfaceBase::WriteTrainFileFull() {
  WriteTrainFile(strTrainfileBuffer);
  strTrainfileBuffer = "";
}

void CDasherInterfaceBase::WriteTrainFilePartial() {

  // FIXME - what if we're midway through a unicode character?

  WriteTrainFile(strTrainfileBuffer.substr(0,100));
  strTrainfileBuffer = strTrainfileBuffer.substr(100);
}

void CDasherInterfaceBase::RequestFullRedraw() {
  SetBoolParameter( BP_REDRAW, true );
}

void CDasherInterfaceBase::AddAlphabetFilename(std::string Filename) {
  m_AlphabetFilenames.push_back(Filename);
}

void CDasherInterfaceBase::AddColourFilename(std::string Filename) {
  m_ColourFilenames.push_back(Filename);
}

void CDasherInterfaceBase::CreateDasherModel() {
  int lmID = GetLongParameter(LP_LANGUAGE_MODEL_ID);

  //  if(m_DashEditbox != 0 && lmID != -1) { We don't need an edit box any more
  if( lmID != -1 ) {

    // Delete the old model and create a new one

    if(m_pDasherModel != NULL) {
      delete m_pDasherModel;
    }

    m_pDasherModel = new CDasherModel(m_pEventHandler, m_pSettingsStore, this);

    // Train the new language model

    string T = m_Alphabet->GetTrainingFile();

    TrainFile(GetStringParameter(SP_SYSTEM_LOC) + T);
    TrainFile(GetStringParameter(SP_USER_LOC) + T);

    // Set various parameters

    //m_pDasherModel->SetControlMode(m_ControlMode);

    if(GetLongParameter(LP_VIEW_ID) != -1)
      ChangeView(GetLongParameter(LP_VIEW_ID));
  }
}

void CDasherInterfaceBase::Start() {
  //m_Paused=false;
  //    SetBoolParameter(BP_DASHER_PAUSED, true);

  PauseAt(0, 0);

  if(m_pDasherModel != 0) {
    m_pDasherModel->Start();
    //    m_pDasherModel->Set_paused(m_Paused);
  }
  if(m_pDasherView != 0) {
    m_pDasherView->ResetSum();
    m_pDasherView->ResetSumCounter();
    m_pDasherView->ResetYAutoOffset();
  }
}

void CDasherInterfaceBase::PauseAt(int MouseX, int MouseY) {
  SetBoolParameter(BP_DASHER_PAUSED, true);

  if(GetBoolParameter(BP_MOUSEPOS_MODE)) {
    SetLongParameter(LP_MOUSE_POS_BOX, 1);
  }

  // Request a full redraw at the next time step.
  SetBoolParameter(BP_REDRAW, true);

  Dasher::CStopEvent oEvent;
  m_pEventHandler->InsertEvent(&oEvent);

  if (m_pUserLog != NULL)
	  m_pUserLog->StopWriting((float) GetNats());
}

void CDasherInterfaceBase::Halt() {
  SetBoolParameter(BP_DASHER_PAUSED, true);

  if(GetBoolParameter(BP_MOUSEPOS_MODE)) {
    SetLongParameter(LP_MOUSE_POS_BOX, 1);
  }


  // This will cause us to reinitialise the frame rate counter - ie we start off slowly
  if(m_pDasherModel != 0)
    m_pDasherModel->Halt();
}

void CDasherInterfaceBase::Unpause(unsigned long Time) {
  SetBoolParameter(BP_DASHER_PAUSED, false);

  if(m_pDasherModel != 0) {
    m_pDasherModel->Reset_framerate(Time);
    //m_pDasherModel->Set_paused(m_Paused);
  }
  if(m_pDasherView != 0) {
    m_pDasherView->ResetSum();
    m_pDasherView->ResetSumCounter();
  }

  SetLongParameter(LP_MOUSE_POS_BOX, -1);

  Dasher::CStartEvent oEvent;
  m_pEventHandler->InsertEvent(&oEvent);

  ResetNats();
  if (m_pUserLog != NULL)
	  m_pUserLog->StartWriting();
}

void CDasherInterfaceBase::Redraw() {
  if(m_pDasherView != 0) {
    m_pDasherView->Render();
    m_pDasherView->Display();
  }

}

void CDasherInterfaceBase::Redraw(int iMouseX, int iMouseY) {
  if(m_pDasherView != 0) {
    if(m_pDasherView->Render(iMouseX, iMouseY, false))  // Only call display if something changed
      m_pDasherView->Display();
  }
}

void CDasherInterfaceBase::SetInput(CDasherInput *_pInput) {
  m_pInput = _pInput;

  if(m_pDasherView != 0)
    m_pDasherView->SetInput(_pInput);
}

void CDasherInterfaceBase::NewFrame(unsigned long iTime) {



  if(GetBoolParameter(BP_REDRAW)) {
    Redraw();
    SetBoolParameter(BP_REDRAW, false);
  }

  if(GetBoolParameter(BP_DASHER_PAUSED) || GetBoolParameter(BP_TRAINING)) {
    DrawMousePos(0, 0, 0);
  }
  else {
    TapOn(0, 0, iTime);
  }

  // Deal with start on mouse position

  if(GetBoolParameter(BP_MOUSEPOS_MODE)) {
    if(m_pDasherView)
      if(m_pDasherView->HandleStartOnMouse(iTime))
        Unpause(iTime);
  }

  //
  // Things which used to be here, but aren't any more:
  //
  //  - finalisation of training thread (needs to be back in UI code)
  //  - mouse gain (needs to be in view/input device)
  //  - start on mouse pos (implement in core, but in a second function)
  //
}

void CDasherInterfaceBase::TapOn(int MouseX, int MouseY, unsigned long Time) {
  if(m_pDasherView != 0) {

    if (m_pUserLog != NULL) {
      
      Dasher::VECTOR_SYMBOL_PROB vAdded;
      int iNumDeleted = 0;

      m_pDasherView->TapOnDisplay(MouseX, MouseY, Time, &vAdded, &iNumDeleted);

      if (iNumDeleted > 0)
        m_pUserLog->DeleteSymbols(iNumDeleted);
      if (vAdded.size() > 0)
        m_pUserLog->AddSymbols(&vAdded);

    }
    else {
      // If there is no user logging going on, we don't need to track the symbols added or deleted.
      m_pDasherView->TapOnDisplay(MouseX, MouseY, Time);
    }

    m_pDasherView->Render(MouseX, MouseY, true);
    m_pDasherView->Display();
  }

  if(m_pDasherModel != 0)
    m_pDasherModel->NewFrame(Time);
}

void CDasherInterfaceBase::ClickTo(int x, int y, int width, int height)
{
    //m_InZoom = 1;
    m_pDasherView->ClickTo(x, y, width, height);
    //m_InZoom = 0;
    
}

void CDasherInterfaceBase::DrawMousePos(int iMouseX, int iMouseY, int iWhichBox) {
  m_pDasherView->Render(iMouseX, iMouseY, false);
  //if (iWhichBox!=-1)
  //m_pDasherView->DrawMousePosBox(iWhichBox, m_iMousePosDist);

  m_pDasherView->Display();
}

void CDasherInterfaceBase::GoTo(int MouseX, int MouseY) {
  if(m_pDasherView != 0) {
    m_pDasherView->GoTo(MouseX, MouseY);
    m_pDasherView->Render();
    m_pDasherView->Display();
  }
}

void CDasherInterfaceBase::DrawGoTo(int MouseX, int MouseY) {
  if(m_pDasherView != 0) {
//              m_pDasherView->Render(MouseY,MouseY);
    m_pDasherView->DrawGoTo(MouseX, MouseY);
    m_pDasherView->Display();
  }
}

void CDasherInterfaceBase::ChangeAlphabet(const std::string &NewAlphabetID) {
  // Don't bother doing any of this if it's the same alphabet
  //  if (GetStringParameter(SP_ALPHABET_ID) != NewAlphabetID) { 

  //     SetStringParameter(SP_ALPHABET_ID, NewAlphabetID); 

  // FIXME - we shouldn't rely on the first call to ChangeAlphabet to
  // construct the list of filenames - we may need to populate a list
  // dialogue before this happens - also, what happens if the list of
  // alphabet files changes at runtime?

  // Update the training file first

  WriteTrainFileFull();

  // Lock Dasher to prevent changes from happening while we're training.

  SetBoolParameter( BP_TRAINING, true );

  if(!m_AlphIO)
    m_AlphIO = new CAlphIO(GetStringParameter(SP_SYSTEM_LOC), GetStringParameter(SP_USER_LOC), m_AlphabetFilenames);

  m_AlphInfo = m_AlphIO->GetInfo(NewAlphabetID);

  //AlphabetID = m_AlphInfo.AlphID.c_str();

  std::auto_ptr < CAlphabet > ptrOld(m_Alphabet);       // So we can delete the old alphabet later

  m_Alphabet = new CAlphabet(m_AlphInfo);

  // Let our user log object know about the new alphabet since
  // it needs to convert symbols into text for the log file.
  if (m_pUserLog != NULL)
    m_pUserLog->SetAlphabetPtr(m_Alphabet);

  // Apply options from alphabet

  SetStringParameter(SP_TRAIN_FILE, m_Alphabet->GetTrainingFile());



  if((m_Alphabet->GetGameModeFile()).length() > 0)
    SetStringParameter(SP_GAME_TEXT_FILE, m_Alphabet->GetGameModeFile());
  

  // DJW_TODO - control mode
  //   if (m_ControlMode==true) {
  //   m_Alphabet->AddControlSymbol();
  //  }

  // Recreate widgets and language model
  if(m_DashEditbox != 0)
    m_DashEditbox->SetInterface(this);
  if(m_DasherScreen != 0)
    m_DasherScreen->SetInterface(this);

  delete m_pDasherModel;
  m_pDasherModel = 0;
  CreateDasherModel();

  if(m_Alphabet->GetPalette() != std::string("") && GetBoolParameter(BP_PALETTE_CHANGE)) {
    //    ChangeColours(m_Alphabet->GetPalette());
    SetStringParameter(SP_COLOUR_ID, m_Alphabet->GetPalette());
  }

  SetBoolParameter( BP_TRAINING, false );

  Start();
  //}
}

std::string CDasherInterfaceBase::GetCurrentAlphabet() {
  return GetStringParameter(SP_ALPHABET_ID);
}

void CDasherInterfaceBase::ChangeColours(const std::string &NewColourID) {
  if(!m_ColourIO)
    m_ColourIO = new CColourIO(GetStringParameter(SP_SYSTEM_LOC), GetStringParameter(SP_USER_LOC), m_ColourFilenames);
  m_ColourInfo = m_ColourIO->GetInfo(NewColourID);

  // delete old colours on editing function
  std::auto_ptr < CCustomColours > ptrColours(m_pColours);

  m_pColours = new CCustomColours(m_ColourInfo);

  //ColourID=m_ColourInfo.ColourID;

  //    SetStringParameter(SP_COLOUR_ID, NewColourID);

  if(m_DasherScreen != 0) {
    m_DasherScreen->SetColourScheme(m_pColours);
  }
}

void CDasherInterfaceBase::ChangeLanguageModel(int NewLanguageModelID) {

  if(NewLanguageModelID != GetLongParameter(LP_LANGUAGE_MODEL_ID)) {
    SetLongParameter(LP_LANGUAGE_MODEL_ID, NewLanguageModelID);

    if(m_Alphabet != 0) {
      CreateDasherModel();

      // We need to call start here so that the root is recreated,
      // otherwise it will fail (this is probably something which
      // needs to be fixed in a more integrated way)
      Start();

    }
  }
}

void CDasherInterfaceBase::ChangeScreen() {
  if(m_pDasherView != 0) {
    m_pDasherView->ChangeScreen(m_DasherScreen);
  } else {
    if(GetLongParameter(LP_VIEW_ID) != -1)
      ChangeView(GetLongParameter(LP_VIEW_ID));
  }
}

void CDasherInterfaceBase::ChangeScreen(CDasherScreen *NewScreen) {
  m_DasherScreen = NewScreen;
  m_DasherScreen->SetColourScheme(m_pColours);
  m_DasherScreen->SetInterface(this);
  ChangeScreen();
  Redraw();
}

void CDasherInterfaceBase::ChangeView(unsigned int NewViewID) {
  //TODO Use DasherViewID

  // FIXME - this shouldn't be here
  SetLongParameter(LP_VIEW_ID, NewViewID);

  if(m_DasherScreen != 0 && m_pDasherModel != 0) {
    delete m_pDasherView;
    m_pDasherView = new CDasherViewSquare(m_pEventHandler, m_pSettingsStore, m_DasherScreen, m_pDasherModel);

    m_pDasherView->SetInput(m_pInput);
  }
}

unsigned int CDasherInterfaceBase::GetNumberSymbols() {
  if(m_Alphabet != 0)
    return m_Alphabet->GetNumberSymbols();
  else
    return 0;
}

const string & CDasherInterfaceBase::GetDisplayText(symbol Symbol) {
  if(m_Alphabet != 0)
    return m_Alphabet->GetDisplayText(Symbol);
  else
    return EmptyString;
}

const string & CDasherInterfaceBase::GetEditText(symbol Symbol) {
  if(m_Alphabet != 0)
    return m_Alphabet->GetText(Symbol);
  else
    return EmptyString;
}

int CDasherInterfaceBase::GetTextColour(symbol Symbol) {
  if(m_Alphabet != 0)
    return m_Alphabet->GetTextColour(Symbol);
  else
    return 4;                   // Default colour for text
}

Opts::ScreenOrientations CDasherInterfaceBase::GetAlphabetOrientation() {
  return m_Alphabet->GetOrientation();
}

Opts::AlphabetTypes CDasherInterfaceBase::GetAlphabetType() {
  return m_Alphabet->GetType();
}

const std::string CDasherInterfaceBase::GetTrainFile() {
  // DOES NOT RETURN FULLY QUALIFIED PATH - SEPARATE SETTING FOR PATH
  return GetStringParameter(SP_TRAIN_FILE);
}

void CDasherInterfaceBase::GetAlphabets(std::vector <std::string >*AlphabetList) {
  if(!m_AlphIO)
    m_AlphIO = new CAlphIO(GetStringParameter(SP_SYSTEM_LOC), GetStringParameter(SP_USER_LOC), m_AlphabetFilenames);
  m_AlphIO->GetAlphabets(AlphabetList);
}

const CAlphIO::AlphInfo & CDasherInterfaceBase::GetInfo(const std::string &AlphID) {
  if(!m_AlphIO)
    m_AlphIO = new CAlphIO(GetStringParameter(SP_SYSTEM_LOC), GetStringParameter(SP_USER_LOC), m_AlphabetFilenames);

  return m_AlphIO->GetInfo(AlphID);
}

void CDasherInterfaceBase::SetInfo(const CAlphIO::AlphInfo &NewInfo) {
  if(!m_AlphIO)
    m_AlphIO = new CAlphIO(GetStringParameter(SP_SYSTEM_LOC), GetStringParameter(SP_USER_LOC), m_AlphabetFilenames);

  m_AlphIO->SetInfo(NewInfo);
}

void CDasherInterfaceBase::DeleteAlphabet(const std::string &AlphID) {
  if(!m_AlphIO)
    m_AlphIO = new CAlphIO(GetStringParameter(SP_SYSTEM_LOC), GetStringParameter(SP_USER_LOC), m_AlphabetFilenames);

  m_AlphIO->Delete(AlphID);
}

void CDasherInterfaceBase::GetColours(std::vector <std::string >*ColourList) {
  if(!m_ColourIO)
    m_ColourIO = new CColourIO(GetStringParameter(SP_SYSTEM_LOC), GetStringParameter(SP_USER_LOC), m_ColourFilenames);
  m_ColourIO->GetColours(ColourList);
}

void CDasherInterfaceBase::ChangeEdit() {
  CreateDasherModel();
  Start();
  Redraw();
}

void CDasherInterfaceBase::ChangeEdit(CDashEditbox *NewEdit) {
  m_DashEditbox = NewEdit;
  //  m_DashEditbox->SetFont(GetStringParameter(SP_EDIT_FONT), GetLongParameter(LP_EDIT_FONT_SIZE));
  m_DashEditbox->SetInterface(this);
  m_DashEditbox->New("");
  ChangeEdit();
}


void CDasherInterfaceBase::Train(string *TrainString, bool IsMore) {
//      m_pDasherModel->LearnText(TrainContext, TrainString, IsMore);
  return;
}

/*
	I've used C style I/O because I found that C++ style I/O bloated
	the Win32 code enormously. The overhead of loading the buffer into
	the string instead of reading straight into a string seems to be
	negligible compared to huge requirements elsewhere.
*/
void CDasherInterfaceBase::TrainFile(string Filename) {
  if(Filename == "")
    return;

  FILE *InputFile;
  if((InputFile = fopen(Filename.c_str(), "r")) == (FILE *) 0)
    return;

  const int BufferSize = 1024;
  char InputBuffer[BufferSize];
  string StringBuffer;
  int NumberRead;

  vector < symbol > Symbols;

  CDasherModel::CTrainer * pTrainer = m_pDasherModel->GetTrainer();
  do {
    NumberRead = fread(InputBuffer, 1, BufferSize - 1, InputFile);
    InputBuffer[NumberRead] = '\0';
    StringBuffer += InputBuffer;
    bool bIsMore = false;
    if(NumberRead == (BufferSize - 1))
      bIsMore = true;

    Symbols.clear();
    m_Alphabet->GetSymbols(&Symbols, &StringBuffer, bIsMore);

    pTrainer->Train(Symbols);

  } while(NumberRead == BufferSize - 1);

  delete pTrainer;

  fclose(InputFile);

}

void CDasherInterfaceBase::GetFontSizes(std::vector <int >*FontSizes) const {
  FontSizes->push_back(20);
  FontSizes->push_back(14);
  FontSizes->push_back(11);
  FontSizes->push_back(40);
  FontSizes->push_back(28);
  FontSizes->push_back(22);
  FontSizes->push_back(80);
  FontSizes->push_back(56);
  FontSizes->push_back(44);
}

double CDasherInterfaceBase::GetCurCPM() {
  //
  return 0;
}

double CDasherInterfaceBase::GetCurFPS() {
  //
  return 0;
}

void CDasherInterfaceBase::AddControlTree(ControlTree *controltree) {
  m_pDasherModel->NewControlTree(controltree);
}

void CDasherInterfaceBase::Render() {
//  if (m_pDasherView!=0)
  //   m_pDasherView->Render();
}

int CDasherInterfaceBase::GetAutoOffset() {
  if(m_pDasherView != 0) {
    return m_pDasherView->GetAutoOffset();
  }
  return -1;
}

double CDasherInterfaceBase::GetNats() const {
  if(m_pDasherModel)
    return m_pDasherModel->GetNats();
  else
    return 0.0;
}

void CDasherInterfaceBase::ResetNats() {
  if(m_pDasherModel)
    m_pDasherModel->ResetNats();
}


void CDasherInterfaceBase::SetContext(std::string strNewContext) {

  // We keep track of an internal context and compare that to what
  // we are given - don't restart Dasher if nothing has changed.
  // This should really be integrated with DasherModel, which
  // probably will be the case when we start to deal with being able
  // to back off indefinitely. For now though we'll keep it in a
  // separate string.

  int iContextLength( 6 ); // The 'important' context length - should really get from language model

  // FIXME - use unicode lengths

  if( strNewContext.substr( std::max(static_cast<int>(strNewContext.size()) - iContextLength, 0)) != strCurrentContext.substr( std::max(static_cast<int>(strCurrentContext.size()) - iContextLength, 0))) {


    if(m_pDasherModel != NULL) {
      if(m_pDasherModel->m_bContextSensitive) {
	m_pDasherModel->SetContext(strNewContext);
	PauseAt(0,0);
      }
    }
    
    strCurrentContext = strNewContext;

    WriteTrainFileFull();
  }
}

// Control mode stuff

void CDasherInterfaceBase::RegisterNode( int iID, const std::string &strLabel, int iColour ) {
  m_pDasherModel->RegisterNode(iID, strLabel, iColour);
}

void CDasherInterfaceBase::ConnectNode(int iChild, int iParent, int iAfter) {
  m_pDasherModel->ConnectNode(iChild, iParent, iAfter);
}

void CDasherInterfaceBase::SetBoolParameter(int iParameter, bool bValue) {
  m_pSettingsStore->SetBoolParameter(iParameter, bValue);
};

void CDasherInterfaceBase::SetLongParameter(int iParameter, long lValue) {
  m_pSettingsStore->SetLongParameter(iParameter, lValue);
};

void CDasherInterfaceBase::SetStringParameter(int iParameter, const std::string & sValue) {
  m_pSettingsStore->SetStringParameter(iParameter, sValue);
};

bool CDasherInterfaceBase::GetBoolParameter(int iParameter) {
  return m_pSettingsStore->GetBoolParameter(iParameter);
}

long CDasherInterfaceBase::GetLongParameter(int iParameter) {
  return m_pSettingsStore->GetLongParameter(iParameter);
}

std::string CDasherInterfaceBase::GetStringParameter(int iParameter) {
  return m_pSettingsStore->GetStringParameter(iParameter);
}

void CDasherInterfaceBase::ResetParameter(int iParameter) {
  m_pSettingsStore->ResetParameter(iParameter);
}

// We need to be able to get at the UserLog object from outside the interface
CUserLog* CDasherInterfaceBase::GetUserLogPtr() {
	return m_pUserLog;
}
