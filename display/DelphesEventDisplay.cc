#include <cassert>
#include <iostream>
#include <utility>
#include <algorithm>
#include "TGeoManager.h"
#include "TGeoVolume.h"
#include "external/ExRootAnalysis/ExRootConfReader.h"
#include "external/ExRootAnalysis/ExRootTreeReader.h"
#include "display/DelphesCaloData.h"
#include "display/DelphesBranchElement.h"
#include "display/Delphes3DGeometry.h"
#include "display/DelphesEventDisplay.h"
#include "classes/DelphesClasses.h"
#include "TEveElement.h"
#include "TEveJetCone.h"
#include "TEveTrack.h"
#include "TEveTrackPropagator.h"
#include "TEveCalo.h"
#include "TEveManager.h"
#include "TEveGeoNode.h"
#include "TEveTrans.h"
#include "TEveViewer.h"
#include "TEveBrowser.h"
#include "TEveArrow.h"
#include "TMath.h"
#include "TSystem.h"
#include "TRootBrowser.h"
#include "TGButton.h"
#include "TClonesArray.h"

DelphesEventDisplay::DelphesEventDisplay()
{
   event_id = 0;
   tkRadius_ = 1.29;
   totRadius_ = 2.0;
   tkHalfLength_ = 3.0;
   bz_ = 3.8;
   chain_ = new TChain("Delphes");
   gTreeReader = 0;
   gDelphesDisplay = 0;
}

DelphesEventDisplay::~DelphesEventDisplay()
{
   delete chain_;
}

DelphesEventDisplay::DelphesEventDisplay(const char *configFile, const char *inputFile, Delphes3DGeometry& det3D)
{
   event_id = 0;
   tkRadius_ = 1.29;
   totRadius_ = 2.0;
   tkHalfLength_ = 3.0;
   bz_ = 3.8;
   chain_ = new TChain("Delphes");
   gTreeReader = 0;
   gDelphesDisplay = 0;

   // initialize the application
   TEveManager::Create(kTRUE, "IV");
   TGeoManager* geom = gGeoManager;

   // build the detector
   tkRadius_ = det3D.getTrackerRadius();
   totRadius_ = det3D.getDetectorRadius();
   tkHalfLength_ = det3D.getTrackerHalfLength();
   bz_ = det3D.getBField();

   //TODO specific to some classical detector... could use better the det3D
   TGeoVolume* top = det3D.getDetector(false);
   geom->SetTopVolume(top);
   TEveElementList *geometry = new TEveElementList("Geometry");
   TEveGeoTopNode* trk = new TEveGeoTopNode(gGeoManager, top->FindNode("tracker_1"));
   trk->SetVisLevel(6);
   geometry->AddElement(trk);
   TEveGeoTopNode* calo = new TEveGeoTopNode(gGeoManager, top->FindNode("Calorimeter_barrel_1"));
   calo->SetVisLevel(3);
   geometry->AddElement(calo);
   calo = new TEveGeoTopNode(gGeoManager, top->FindNode("Calorimeter_endcap_1"));
   calo->SetVisLevel(3);
   calo->UseNodeTrans();
   geometry->AddElement(calo);
   calo = new TEveGeoTopNode(gGeoManager, top->FindNode("Calorimeter_endcap_2"));
   calo->SetVisLevel(3);
   calo->UseNodeTrans();
   geometry->AddElement(calo);
   TEveGeoTopNode* muon = new TEveGeoTopNode(gGeoManager, top->FindNode("muons_barrel_1"));
   muon->SetVisLevel(4);
   geometry->AddElement(muon);
   muon = new TEveGeoTopNode(gGeoManager, top->FindNode("muons_endcap_1"));
   muon->SetVisLevel(4);
   muon->UseNodeTrans();
   geometry->AddElement(muon);
   muon = new TEveGeoTopNode(gGeoManager, top->FindNode("muons_endcap_2"));
   muon->SetVisLevel(4);
   muon->UseNodeTrans();
   geometry->AddElement(muon);
   //gGeoManager->DefaultColors();

   // Create chain of root trees
   chain_->Add(inputFile);

   // Create object of class ExRootTreeReader
   printf("*** Opening Delphes data file ***\n");
   gTreeReader = new ExRootTreeReader(chain_);

   // prepare data collections
   readConfig(configFile, det3D, gElements, gArrays);
   for(std::vector<DelphesBranchBase*>::iterator element = gElements.begin(); element<gElements.end(); ++element) {
     DelphesBranchElement<TEveTrackList>*   item_v1 = dynamic_cast<DelphesBranchElement<TEveTrackList>*>(*element);
     DelphesBranchElement<DelphesCaloData>* item_v2 = dynamic_cast<DelphesBranchElement<DelphesCaloData>*>(*element);
     DelphesBranchElement<TEveElementList>* item_v3 = dynamic_cast<DelphesBranchElement<TEveElementList>*>(*element);
     if(item_v1) gEve->AddElement(item_v1->GetContainer());
     if(item_v2) gEve->AddElement(item_v2->GetContainer());
     if(item_v3) gEve->AddElement(item_v3->GetContainer());
   }

   // viewers and scenes
   gDelphesDisplay = new DelphesDisplay;
   gEve->AddGlobalElement(geometry);
   gDelphesDisplay->ImportGeomRPhi(geometry);
   gDelphesDisplay->ImportGeomRhoZ(geometry);
   // find the first calo data and use that to initialize the calo display
   for(std::vector<DelphesBranchBase*>::iterator data=gElements.begin();data<gElements.end();++data) {
     if(TString((*data)->GetType())=="tower") {
       DelphesCaloData* container = dynamic_cast<DelphesBranchElement<DelphesCaloData>*>((*data))->GetContainer();
       assert(container);
       TEveCalo3D *calo3d = new TEveCalo3D(container);
       calo3d->SetBarrelRadius(tkRadius_);
       calo3d->SetEndCapPos(tkHalfLength_);
       gEve->AddGlobalElement(calo3d);
       gDelphesDisplay->ImportCaloRPhi(calo3d);
       gDelphesDisplay->ImportCaloRhoZ(calo3d);
       TEveCaloLego *lego = new TEveCaloLego(container);
       lego->InitMainTrans();
       lego->RefMainTrans().SetScale(TMath::TwoPi(), TMath::TwoPi(), TMath::Pi());
       lego->SetAutoRebin(kFALSE);
       lego->Set2DMode(TEveCaloLego::kValSizeOutline);
       gDelphesDisplay->ImportCaloLego(lego);
       break;
     }
   }

   //make_gui();
   //load_event();
   gEve->Redraw3D(kTRUE);   

}
// function that parses the config to extract the branches of interest and prepare containers
void DelphesEventDisplay::readConfig(const char *configFile, Delphes3DGeometry& det3D, std::vector<DelphesBranchBase*>& elements, std::vector<TClonesArray*>& arrays) {
   ExRootConfReader *confReader = new ExRootConfReader;
   confReader->ReadFile(configFile);
   Double_t tk_radius = det3D.getTrackerRadius();
   Double_t tk_length = det3D.getTrackerHalfLength();
   Double_t tk_Bz     = det3D.getBField();
   Double_t mu_radius = det3D.getDetectorRadius();
   Double_t mu_length = det3D.getDetectorHalfLength();
   TAxis*   etaAxis   = det3D.getCaloAxes().first;
   TAxis*   phiAxis   = det3D.getCaloAxes().second;
   ExRootConfParam branches = confReader->GetParam("TreeWriter::Branch");
   Int_t nBranches = branches.GetSize()/3;
   DelphesBranchElement<TEveTrackList>* tlist;
   DelphesBranchElement<DelphesCaloData>* clist;
   DelphesBranchElement<TEveElementList>* elist;
   for(Int_t b = 0; b<nBranches; ++b) {
     TString input = branches[b*3].GetString();
     TString name = branches[b*3+1].GetString();
     TString className = branches[b*3+2].GetString();
     if(className=="Track") {
       if(input.Contains("eflow",TString::kIgnoreCase) || name.Contains("eflow",TString::kIgnoreCase)) continue; //no eflow
       tlist = new DelphesBranchElement<TEveTrackList>(name,"track",kBlue);
       elements.push_back(tlist);
       TEveTrackPropagator *trkProp = tlist->GetContainer()->GetPropagator();
       trkProp->SetMagField(0., 0., -tk_Bz);
       trkProp->SetMaxR(tk_radius);
       trkProp->SetMaxZ(tk_length);
     } else if(className=="Tower") {
       if(input.Contains("eflow",TString::kIgnoreCase) || name.Contains("eflow",TString::kIgnoreCase)) continue; //no eflow
       clist = new DelphesBranchElement<DelphesCaloData>(name,"tower",kBlack);
       clist->GetContainer()->SetEtaBins(etaAxis);
       clist->GetContainer()->SetPhiBins(phiAxis);
       elements.push_back(clist);
     } else if(className=="Jet") {
       if(input.Contains("GenJetFinder")) {
         elist = new DelphesBranchElement<TEveElementList>(name,"jet",kCyan);
         elist->GetContainer()->SetRnrSelf(false);
         elist->GetContainer()->SetRnrChildren(false);
         elements.push_back(elist);
       } else {
         elements.push_back(new DelphesBranchElement<TEveElementList>(name,"jet",kYellow));
       }
     } else if(className=="Electron") {
       tlist = new DelphesBranchElement<TEveTrackList>(name,"track",kRed);
       elements.push_back(tlist);
       TEveTrackPropagator *trkProp = tlist->GetContainer()->GetPropagator();
       trkProp->SetMagField(0., 0., -tk_Bz);
       trkProp->SetMaxR(tk_radius);
       trkProp->SetMaxZ(tk_length);
     } else if(className=="Photon") {
       tlist = new DelphesBranchElement<TEveTrackList>(name,"photon",kYellow);
       elements.push_back(tlist);
       TEveTrackPropagator *trkProp = tlist->GetContainer()->GetPropagator();
       trkProp->SetMagField(0., 0., 0.);
       trkProp->SetMaxR(tk_radius);
       trkProp->SetMaxZ(tk_length);
     } else if(className=="Muon") {
       tlist = new DelphesBranchElement<TEveTrackList>(name,"track",kGreen);
       elements.push_back(tlist);
       TEveTrackPropagator *trkProp = tlist->GetContainer()->GetPropagator();
       trkProp->SetMagField(0., 0., -tk_Bz);
       trkProp->SetMaxR(mu_radius);
       trkProp->SetMaxZ(mu_length);
     } else if(className=="MissingET") {
       elements.push_back(new DelphesBranchElement<TEveElementList>(name,"vector",kViolet));
     } else if(className=="GenParticle") {
       tlist = new DelphesBranchElement<TEveTrackList>(name,"track",kCyan);
       elements.push_back(tlist);
       tlist->GetContainer()->SetRnrSelf(false);
       tlist->GetContainer()->SetRnrChildren(false);
       TEveTrackPropagator *trkProp = tlist->GetContainer()->GetPropagator();
       trkProp->SetMagField(0., 0., -tk_Bz);
       trkProp->SetMaxR(tk_radius);
       trkProp->SetMaxZ(tk_length);
     }
//TODO one possible simplification could be to add the array to the element class.
     arrays.push_back(gTreeReader->UseBranch(name));
   }
}


//______________________________________________________________________________
void DelphesEventDisplay::load_event()
{
   // Load event specified in global event_id.
   // The contents of previous event are removed.

   //TODO move this to the status bar ???
   printf("Loading event %d.\n", event_id);


   // clear the previous event
   gEve->GetViewers()->DeleteAnnotations();
   for(std::vector<DelphesBranchBase*>::iterator data=gElements.begin();data<gElements.end();++data) {
     (*data)->Reset();
   }

   // read the new event
   delphes_read();

//TODO: it blocks somewhere below....
//TODO: also the event content has one weird entry "TEveCalData" -> corruption????
//other observation: projections appear in the 3D view ! -> seems to indicate that we have a common problem there.
//should check the content of the elements vector when filling them
   // update display
   TEveElement* top = (TEveElement*)gEve->GetCurrentEvent();
   gDelphesDisplay->DestroyEventRPhi();
   gDelphesDisplay->ImportEventRPhi(top);
   gDelphesDisplay->DestroyEventRhoZ();
   gDelphesDisplay->ImportEventRhoZ(top);
   //update_html_summary();
   gEve->Redraw3D(kFALSE, kTRUE);
}

void DelphesEventDisplay::delphes_read()
{

  // safety
  if(event_id >= gTreeReader->GetEntries() || event_id<0 ) return;

  // Load selected branches with data from specified event
  gTreeReader->ReadEntry(event_id);

  // loop over selected branches, and apply the proper recipe to fill the collections.
  // this is basically to loop on gArrays to fill gElements.

//TODO: one option would be to have templated methods in the element classes. We could simply call "element.fill()"
  std::vector<TClonesArray*>::iterator data = gArrays.begin();
  std::vector<DelphesBranchBase*>::iterator element = gElements.begin();
  std::vector<TClonesArray*>::iterator data_tracks = gArrays.begin();
  std::vector<DelphesBranchBase*>::iterator element_tracks = gElements.begin();
  Int_t nTracks = 0;
  for(; data<gArrays.end() && element<gElements.end(); ++data, ++element) {
    TString type = (*element)->GetType();
    // keep the most generic track collection for the end
    if(type=="track" && TString((*element)->GetClassName())=="Track" && nTracks==0) {
      data_tracks = data;
      element_tracks = element;
      nTracks = (*data_tracks)->GetEntries();
      continue;
    }
    // branch on the element type
    if(type=="tower") delphes_read_towers(*data,*element);
    else if(type=="track" || type=="photon") delphes_read_tracks(*data,*element);
    else if(type=="jet") delphes_read_jets(*data,*element);
    else if(type=="vector") delphes_read_vectors(*data,*element);
  }
  // finish whith what we consider to be the main track collection
  if(nTracks>0) delphes_read_tracks(*data,*element);
}

void DelphesEventDisplay::delphes_read_towers(TClonesArray* data, DelphesBranchBase* element) {
  DelphesCaloData* container = dynamic_cast<DelphesBranchElement<DelphesCaloData>*>(element)->GetContainer();
  assert(container);
  // Loop over all towers
  TIter itTower(data);
  Tower *tower;
  while((tower = (Tower *) itTower.Next()))
  {
    container->AddTower(tower->Edges[0], tower->Edges[1], tower->Edges[2], tower->Edges[3]);
    container->FillSlice(0, tower->Eem);
    container->FillSlice(1, tower->Ehad);
  }
  container->DataChanged();
}

void DelphesEventDisplay::delphes_read_tracks(TClonesArray* data, DelphesBranchBase* element) {
  TEveTrackList* container = dynamic_cast<DelphesBranchElement<TEveTrackList>*>(element)->GetContainer();
  assert(container);
  TString className = element->GetClassName();
  TIter itTrack(data);
  Int_t counter = 0;
  TEveTrack *eveTrack;
  TEveTrackPropagator *trkProp = container->GetPropagator();
  if(className=="Track") {
    // Loop over all tracks
    Track *track;
    while((track = (Track *) itTrack.Next())) {
      TParticle pb(track->PID, 1, 0, 0, 0, 0,
                   track->P4().Px(), track->P4().Py(),
                   track->P4().Pz(), track->P4().E(),
                   track->X, track->Y, track->Z, 0.0);

      eveTrack = new TEveTrack(&pb, counter, trkProp);
      eveTrack->SetName(Form("%s [%d]", pb.GetName(), counter++));
      eveTrack->SetStdTitle();
      eveTrack->SetAttLineAttMarker(container);
      container->AddElement(eveTrack);
      eveTrack->SetLineColor(element->GetColor());
      eveTrack->MakeTrack();
    }
  } else if(className=="Electron") {
    // Loop over all electrons
    Electron *electron;
    while((electron = (Electron *) itTrack.Next())) {
      TParticle pb(electron->Charge<0?11:-11, 1, 0, 0, 0, 0,
                   electron->P4().Px(), electron->P4().Py(),
                   electron->P4().Pz(), electron->P4().E(),
                   0., 0., 0., 0.);

      eveTrack = new TEveTrack(&pb, counter, trkProp);
      eveTrack->SetName(Form("%s [%d]", pb.GetName(), counter++));
      eveTrack->SetStdTitle();
      eveTrack->SetAttLineAttMarker(container);
      container->AddElement(eveTrack);
      eveTrack->SetLineColor(element->GetColor());
      eveTrack->MakeTrack();
  }
  } else if(className=="Muon") {
    // Loop over all muons
    Muon *muon;
    while((muon = (Muon *) itTrack.Next())) {
      TParticle pb(muon->Charge<0?13:-13, 1, 0, 0, 0, 0,
                   muon->P4().Px(), muon->P4().Py(),
                   muon->P4().Pz(), muon->P4().E(),
                   0., 0., 0., 0.);

      eveTrack = new TEveTrack(&pb, counter, trkProp);
      eveTrack->SetName(Form("%s [%d]", pb.GetName(), counter++));
      eveTrack->SetStdTitle();
      eveTrack->SetAttLineAttMarker(container);
      container->AddElement(eveTrack);
      eveTrack->SetLineColor(element->GetColor());
      eveTrack->MakeTrack();
    }
  } else if(className=="Photon") {
    // Loop over all photons
    Photon *photon;
    while((photon = (Photon *) itTrack.Next())) {
      TParticle pb(22, 1, 0, 0, 0, 0,
                   photon->P4().Px(), photon->P4().Py(),
                   photon->P4().Pz(), photon->P4().E(),
                   0., 0., 0., 0.);

      eveTrack = new TEveTrack(&pb, counter, trkProp);
      eveTrack->SetName(Form("%s [%d]", pb.GetName(), counter++));
      eveTrack->SetStdTitle();
      eveTrack->SetAttLineAttMarker(container);
      container->AddElement(eveTrack);
      eveTrack->SetLineColor(element->GetColor());
      eveTrack->MakeTrack();
    }
  }
}

void DelphesEventDisplay::delphes_read_jets(TClonesArray* data, DelphesBranchBase* element) {
  TEveElementList* container = dynamic_cast<DelphesBranchElement<TEveElementList>*>(element)->GetContainer();
  assert(container);
  TIter itJet(data);
  Jet *jet;
  TEveJetCone *eveJetCone;
  // Loop over all jets
  Int_t counter = 0;
  while((jet = (Jet *) itJet.Next()))
  {
    eveJetCone = new TEveJetCone();
    eveJetCone->SetTitle(Form("jet [%d]: Pt=%f, Eta=%f, \nPhi=%f, M=%f",counter,jet->PT, jet->Eta, jet->Phi, jet->Mass));
    eveJetCone->SetName(Form("jet [%d]", counter++));
    eveJetCone->SetMainTransparency(60);
    eveJetCone->SetLineColor(element->GetColor());
    eveJetCone->SetFillColor(element->GetColor());
    eveJetCone->SetCylinder(tkRadius_ - 10, tkHalfLength_ - 10);
    eveJetCone->SetPickable(kTRUE);
    eveJetCone->AddEllipticCone(jet->Eta, jet->Phi, jet->DeltaEta, jet->DeltaPhi);
    container->AddElement(eveJetCone);
  }
}

void DelphesEventDisplay::delphes_read_vectors(TClonesArray* data, DelphesBranchBase* element) {
  TEveElementList* container = dynamic_cast<DelphesBranchElement<TEveElementList>*>(element)->GetContainer();
  assert(container);
  TIter itMet(data);
  MissingET *MET;
  TEveArrow *eveMet;
  // Missing Et
  Double_t maxPt = 50.;
  // TODO to be changed as we don't have access to maxPt anymore. MET scale could be a general parameter set in GUI
  while((MET = (MissingET*) itMet.Next())) {
    eveMet = new TEveArrow((tkRadius_ * MET->MET/maxPt)*cos(MET->Phi), (tkRadius_ * MET->MET/maxPt)*sin(MET->Phi), 0., 0., 0., 0.);
    eveMet->SetMainColor(element->GetColor());
    eveMet->SetTubeR(0.04);
    eveMet->SetConeR(0.08);
    eveMet->SetConeL(0.10);
    eveMet->SetPickable(kTRUE);
    eveMet->SetName("Missing Et");
    eveMet->SetTitle(Form("Missing Et (%.1f GeV)",MET->MET));
    container->AddElement(eveMet);
  }
}

/******************************************************************************/
// GUI
/******************************************************************************/

void DelphesEventDisplay::make_gui()
{
   // Create minimal GUI for event navigation.
   // TODO: better GUI could be made based on the ch15 of the manual (Writing a GUI)

   // add a tab on the left
   TEveBrowser* browser = gEve->GetBrowser();
   browser->StartEmbedding(TRootBrowser::kLeft);

   // set the main title
   TGMainFrame* frmMain = new TGMainFrame(gClient->GetRoot(), 1000, 600);
   frmMain->SetWindowName("Delphes Event Display");
   frmMain->SetCleanup(kDeepCleanup);

   // build the navigation menu
   TGHorizontalFrame* hf = new TGHorizontalFrame(frmMain);
   {
      TString icondir;
      if(gSystem->Getenv("ROOTSYS"))
        icondir = Form("%s/icons/", gSystem->Getenv("ROOTSYS"));
      if(!gSystem->OpenDirectory(icondir)) 
        icondir = Form("%s/icons/", (const char*)gSystem->GetFromPipe("root-config --etcdir") );
      TGPictureButton* b = 0;
      EvNavHandler    *fh = new EvNavHandler(this);

      b = new TGPictureButton(hf, gClient->GetPicture(icondir+"GoBack.gif"));
      hf->AddFrame(b);
      b->Connect("Clicked()", "EvNavHandler", fh, "Bck()");

      b = new TGPictureButton(hf, gClient->GetPicture(icondir+"GoForward.gif"));
      hf->AddFrame(b);
      b->Connect("Clicked()", "EvNavHandler", fh, "Fwd()");
   }
   frmMain->AddFrame(hf);
   frmMain->MapSubwindows();
   frmMain->Resize();
   frmMain->MapWindow();
   browser->StopEmbedding();
   browser->SetTabTitle("Event Control", 0);
}
