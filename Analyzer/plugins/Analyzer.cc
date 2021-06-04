// -*- C++ -*-
//
// Package:    SUSYBSMAnalysis/Analyzer
// Class:      Analyzer
//
/**\class Analyzer Analyzer.cc SUSYBSMAnalysis/Analyzer/plugins/Analyzer.cc
*/
//
// Original Author:  Emery Nibigira
//         Created:  Thu, 01 Apr 2021 07:04:53 GMT
//
//


#include "SUSYBSMAnalysis/Analyzer/plugins/Analyzer.h"


Analyzer::Analyzer(const edm::ParameterSet& iConfig)
   // Read config file
   :hscpToken_(consumes<vector<susybsm::HSCParticle>>(iConfig.getParameter<edm::InputTag>("hscpCollection")))
   ,hscpIsoToken_(consumes<edm::ValueMap<susybsm::HSCPIsolation>>(iConfig.getParameter<edm::InputTag>("hscpIsoCollection")))
   ,muonSegmentToken_(consumes<susybsm::MuonSegmentCollection>(iConfig.getParameter<edm::InputTag>("muonSegmentCollection")))
   ,dedxToken_(consumes<reco::DeDxHitInfoAss>(iConfig.getParameter<edm::InputTag>("dedxCollection")))
   ,muonTimeToken_(consumes<reco::MuonTimeExtraMap>(iConfig.getParameter<edm::InputTag>("muonTimeCollection")))
   ,muonDtTimeToken_(consumes<reco::MuonTimeExtraMap>(iConfig.getParameter<edm::InputTag>("muonDtTimeCollection")))
   ,muonCscTimeToken_(consumes<reco::MuonTimeExtraMap>(iConfig.getParameter<edm::InputTag>("muonCscTimeCollection")))
   ,muonDtSegmentToken_(consumes<DTRecSegment4DCollection>(iConfig.getParameter<edm::InputTag>("muonDtSegmentCollection")))
   ,muonCscSegmentToken_(consumes<CSCSegmentCollection>(iConfig.getParameter<edm::InputTag>("muonCscSegmentCollection")))
   ,offlinePrimaryVerticesToken_(consumes<vector<reco::Vertex>>(iConfig.getParameter<edm::InputTag>("offlinePrimaryVerticesCollection")))
   ,refittedStandAloneMuonsToken_(consumes<vector<reco::Track>>(iConfig.getParameter<edm::InputTag>("refittedStandAloneMuonsCollection")))
   ,offlineBeamSpotToken_(consumes<reco::BeamSpot>(iConfig.getParameter<edm::InputTag>("offlineBeamSpotCollection")))
   ,muonToken_(consumes<vector<reco::Muon>>(iConfig.getParameter<edm::InputTag>("muonCollection")))
   ,triggerResultsToken_(consumes<edm::TriggerResults>(iConfig.getParameter<edm::InputTag>("triggerResults")))
   // HLT triggers
   ,trigger_met_(iConfig.getUntrackedParameter<vector<string>>("Trigger_MET"))
   ,trigger_mu_(iConfig.getUntrackedParameter<vector<string>>("Trigger_Mu"))
   // =========Analysis parameters================
   ,TypeMode_(iConfig.getUntrackedParameter<unsigned int>("TypeMode"))
   ,SampleType_(iConfig.getUntrackedParameter<unsigned int>("SampleType"))
   ,SkipSelectionPlot_(iConfig.getUntrackedParameter<bool>("SkipSelectionPlot"))
   ,PtHistoUpperBound(iConfig.getUntrackedParameter<double>("PtHistoUpperBound"))
   ,MassHistoUpperBound(iConfig.getUntrackedParameter<double>("MassHistoUpperBound"))
   ,MassNBins(iConfig.getUntrackedParameter<unsigned int>("MassNBins"))
   ,IPbound(iConfig.getUntrackedParameter<double>("IPbound"))
   ,PredBins(iConfig.getUntrackedParameter<unsigned int>("PredBins"))
   ,EtaBins(iConfig.getUntrackedParameter<unsigned int>("EtaBins"))
   ,dEdxS_UpLim(iConfig.getUntrackedParameter<double>("dEdxS_UpLim"))
   ,dEdxM_UpLim(iConfig.getUntrackedParameter<double>("dEdxM_UpLim"))
   ,DzRegions(iConfig.getUntrackedParameter<unsigned int>("DzRegions"))
   ,GlobalMaxPterr(iConfig.getUntrackedParameter<double>("GlobalMaxPterr"))
   ,GlobalMinPt(iConfig.getUntrackedParameter<double>("GlobalMinPt"))
   ,GlobalMinTOF(iConfig.getUntrackedParameter<double>("GlobalMinTOF"))
   ,skipPixel(iConfig.getUntrackedParameter<bool>("skipPixel")) 
   ,useTemplateLayer(iConfig.getUntrackedParameter<bool>("useTemplateLayer"))
   //,DeDxSF_0(iConfig.getUntrackedParameter<double>("DeDxSF_0"))
   //,DeDxSF_1(iConfig.getUntrackedParameter<double>("DeDxSF_1"))
   //,DeDxK(iConfig.getUntrackedParameter<double>("DeDxK"))
   //,DeDxC(iConfig.getUntrackedParameter<double>("DeDxC"))
   ,DeDxTemplate(iConfig.getUntrackedParameter<string>("DeDxTemplate"))
   ,enableDeDxCalibration(iConfig.getUntrackedParameter<bool>("enableDeDxCalibration"))
   ,DeDxCalibration(iConfig.getUntrackedParameter<string>("DeDxCalibration"))
   ,Geometry(iConfig.getUntrackedParameter<string>("Geometry"))
   ,TimeOffset(iConfig.getUntrackedParameter<string>("TimeOffset"))

{
   //now do what ever initialization is needed
   // define the selection to be considered later for the optimization
   // WARNING: recall that this has a huge impact on the analysis time AND on the output file size --> be carefull with your choice
   
   useClusterCleaning = true;
   if(TypeMode_==4) {
      useClusterCleaning = false; //switch off cluster cleaning for mCHAMPs
   }
   
   isData   = (SampleType_==0);
   isMC     = (SampleType_==1);
   isSignal = (SampleType_>=2);

   //dEdxSF [0] = DeDxSF_0;
   //dEdxSF [1] = DeDxSF_1;

   bool splitByModuleType = true;
   dEdxTemplates = loadDeDxTemplate(DeDxTemplate, splitByModuleType);
   if(enableDeDxCalibration)   trackerCorrector.LoadDeDxCalibration(DeDxCalibration); 
   else                        trackerCorrector.TrackerGains = nullptr; //FIXME check gain for MC

   moduleGeom::loadGeometry(Geometry);
   tofCalculator.loadTimeOffset(TimeOffset);

}


Analyzer::~Analyzer()
{

   // do anything here that needs to be done at desctruction time
   // (e.g. close files, deallocate resources etc.)

}

// ------------ method called once each job just before starting event loop  ------------
void
Analyzer::beginJob()
{
   
   // Book histograms
   edm::Service<TFileService> fs;
   tuple = new Tuple();
   
   string BaseName;
   if(isData)
        BaseName = "Data";
   else BaseName = "MC";

   TFileDirectory dir = fs->mkdir( BaseName.c_str(), BaseName.c_str() );

   // create histograms & trees
   initializeCuts(fs, CutPt_, CutI_, CutTOF_, CutPt_Flip_, CutI_Flip_, CutTOF_Flip_);
   tuple_maker->initializeTuple(tuple, dir, SkipSelectionPlot_, TypeMode_, isSignal, CutPt_.size(), CutPt_Flip_.size(), PtHistoUpperBound, MassHistoUpperBound, MassNBins, IPbound, PredBins, EtaBins, dEdxS_UpLim, dEdxM_UpLim, DzRegions, GlobalMinPt, GlobalMinTOF);

   tuple->IntLumi->Fill(0.0,IntegratedLuminosity_);

   tof    = nullptr;
   dttof  = nullptr;
   csctof = nullptr;

   TrigInfo_ = 0;

   CurrentRun_ = 0;
   RNG = new TRandom3();
   is2016 = false;
   is2016G = false;

   


   HSCPTk              = new bool[CutPt_.size()];
   HSCPTk_SystP        = new bool[CutPt_.size()];
   HSCPTk_SystI        = new bool[CutPt_.size()];
   HSCPTk_SystT        = new bool[CutPt_.size()];
   HSCPTk_SystM        = new bool[CutPt_.size()];
   HSCPTk_SystPU       = new bool[CutPt_.size()];
   HSCPTk_SystHUp      = new bool[CutPt_.size()];
   HSCPTk_SystHDown    = new bool[CutPt_.size()];
   MaxMass           = new double[CutPt_.size()];
   MaxMass_SystP     = new double[CutPt_.size()];
   MaxMass_SystI     = new double[CutPt_.size()];
   MaxMass_SystT     = new double[CutPt_.size()];
   MaxMass_SystM     = new double[CutPt_.size()];
   MaxMass_SystPU    = new double[CutPt_.size()];
   MaxMass_SystHUp   = new double[CutPt_.size()];
   MaxMass_SystHDown = new double[CutPt_.size()];

   /*HIPemulator.    setPeriodHIPRate(is2016G);
   HIPemulatorUp.  setPeriodHIPRate(is2016G, "ratePdfPixel_Up", "ratePdfStrip_Up");
   HIPemulatorDown.setPeriodHIPRate(is2016G, "ratePdfPixel_Up", "ratePdfStrip_Up");*/

   //HIPemulatorUp(false, "ratePdfPixel_Up", "ratePdfStrip_Up");
   //HIPemulatorDown(false, "ratePdfPixel_Down", "ratePdfStrip_Down");
}


//
// member functions
//

// ------------ method called for each event  ------------
void
Analyzer::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup)
{
   using namespace edm;

   //if run change, update conditions
   if(CurrentRun_ != iEvent.id().run()){
      CurrentRun_  = iEvent.id().run();
      tofCalculator.setRun(CurrentRun_);
      trackerCorrector.setRun(CurrentRun_);

      loadDeDxParameters(CurrentRun_, SampleType_, DeDxSF_0, DeDxSF_1, DeDxK, DeDxC);
      dEdxSF [0] = DeDxSF_0;
      dEdxSF [1] = DeDxSF_1;

      LogInfo("Analyzer") <<"------> dEdx parameters SF for run = "<<CurrentRun_<< "  "<< dEdxSF[1];
   }

   //WAIT////compute event weight
   /*if(SampleType>0)
      EventWeight_ = SampleWeight * GetPUWeight(ev, samples[s].Pileup, PUSystFactor, LumiWeightsMC, LumiWeightsMCSyst);
   else
      EventWeight_ = 1;*/
   
   vector<reco::GenParticle> genColl;
   double HSCPGenBeta1=-1, HSCPGenBeta2=-1;
   double HSCPDLength1=-1, HSCPDLength2=-1;
   if(isSignal){
      //get the collection of generated Particles
      Handle< vector<reco::GenParticle> > genCollH;
      iEvent.getByLabel("genParticlePlusGeant", genCollH);
      if(!genCollH.isValid()) {
         iEvent.getByLabel("genParticlesSkimmed", genCollH);
         if(!genCollH.isValid()) iEvent.getByLabel("genParticles", genCollH);
         else LogError("Analyzer") << "GenParticle Collection NotFound";
      }
      genColl = *genCollH;
      int NChargedHSCP=HowManyChargedHSCP(genColl);
      //WAIT//EventWeight_*=samples[s].GetFGluinoWeight(NChargedHSCP);

      GetGenHSCPDecayLength(genColl,HSCPDLength1,HSCPDLength2,true);
      tuple->Gen_DecayLength->Fill(HSCPDLength1, EventWeight_); //????
      tuple->Gen_DecayLength->Fill(HSCPDLength2, EventWeight_);

      GetGenHSCPBeta(genColl,HSCPGenBeta1,HSCPGenBeta2,false);
      if(HSCPGenBeta1>=0)tuple->Beta_Gen->Fill(HSCPGenBeta1, EventWeight_);  
      if(HSCPGenBeta2>=0)tuple->Beta_Gen->Fill(HSCPGenBeta2, EventWeight_);

      GetGenHSCPBeta(genColl,HSCPGenBeta1,HSCPGenBeta2,true);
      if(HSCPGenBeta1>=0)tuple->Beta_GenCharged->Fill(HSCPGenBeta1, EventWeight_); 
      if(HSCPGenBeta2>=0)tuple->Beta_GenCharged->Fill(HSCPGenBeta2, EventWeight_);

      // R-hadron wights needed due to wrong GenId---------------------------------BEGIN
      // ... missing
      // R-hadron wights needed due to wrong GenId---------------------------------END

      for(unsigned int g=0;g<genColl.size();g++) {
         if(genColl[g].pt()<5)continue;
         if(genColl[g].status()!=1)continue;
         int AbsPdg=abs(genColl[g].pdgId());
         if(AbsPdg<1000000 && AbsPdg!=17)continue;

         // categorise event with R-hadrons for additional weighting-----------------------BEGIN
         // ... missing
         // categorise event with R-hadrons for additional weighting-----------------------BEGIN

         tuple->genlevelpT->Fill(genColl[g].pt(), EventWeight_);
         tuple->genleveleta->Fill(genColl[g].eta(), EventWeight_);
         tuple->genlevelbeta->Fill(genColl[g].p()/genColl[g].energy(), EventWeight_);
      }

   } //End of isSignal

   // new genHSCP ntuple after correcting weights
   int nrha=0; 
   for(unsigned int g=0;g<genColl.size();g++) {
      if(genColl[g].pt()<5)continue;
      if(genColl[g].status()!=1)continue;
      int AbsPdg=abs(genColl[g].pdgId());
      if(AbsPdg<1000000 && AbsPdg!=17)continue;

      nrha++;
      //mk rhadron ntuple
      if(isSignal)
		   tuple_maker->fillGenTreeBranches(tuple, iEvent.id().run(),iEvent.id().event(),iEvent.id().luminosityBlock(), nrha, EventWeight_,genColl[g].pdgId(),genColl[g].charge(),genColl[g].mass(),genColl[g].pt(),genColl[g].eta(),genColl[g].phi());

   }
	nrha=0;

   //check if the event is passing trigger
   tuple->TotalE  ->Fill(0.0,EventWeight_);
   tuple->TotalEPU->Fill(0.0,EventWeight_*PUSystFactor_);
   //See if event passed signal triggers
   //WAIT//if(!PassTrigger(iEvent, isData, false, (is2016&&!is2016G)?&L1Emul:nullptr) ) {
   if(!passTrigger(iEvent, isData)){ return;
      //For TOF only analysis if the event doesn't pass the signal triggers check if it was triggered by the no BPTX cosmic trigger
      //If not TOF only then move to next event
      //WAIT//if(TypeMode_!=3) continue;
      //WAIT//if(!PassTrigger(ev, isData, true, (is2016&&!is2016G)?&L1Emul:NULL)) continue;
      //WAIT//if(!passTrigger(iEvent, isData, true)) continue;

      //If is cosmic event then switch plots to use to the ones for cosmics
	   //WAIT//SamplePlots=&plotsMap[CosmicName];
	}
   //WAIT//else if(TypeMode==3) {
	   //WAIT//SamplePlots = &plotsMap[samples[s].Name];
   //WAIT//}

   tuple->TotalTE->Fill(0.0,EventWeight_);

   //keep beta distribution for signal
   if(isSignal){
      if(HSCPGenBeta1>=0) tuple->Beta_Triggered->Fill(HSCPGenBeta1, EventWeight_); 
      if(HSCPGenBeta2>=0) tuple->Beta_Triggered->Fill(HSCPGenBeta2, EventWeight_);
   }
   

   //===================== Handle For DeDx Hits ==============
   Handle<reco::DeDxHitInfoAss> dedxCollH;
   iEvent.getByToken(dedxToken_,dedxCollH);

   //================= Handle For Muon TOF Combined ===============
   Handle<reco::MuonTimeExtraMap>     tofMap;
   iEvent.getByToken(muonTimeToken_,  tofMap);

   //================= Handle For Muon TOF DT ===============
   Handle<reco::MuonTimeExtraMap>       tofDtMap;
   iEvent.getByToken(muonDtTimeToken_,  tofDtMap);

   //================= Handle For Muon TOF CSC ===============
   Handle<reco::MuonTimeExtraMap>        tofCscMap;
   iEvent.getByToken(muonCscTimeToken_,  tofCscMap);

   //================= Handle For Muon DT/CSC Segment ===============
   Handle<CSCSegmentCollection> CSCSegmentCollH;
   Handle<DTRecSegment4DCollection> DTSegmentCollH;
   if(!isMC){ //do not recompute TOF on MC background
      iEvent.getByToken(muonCscSegmentToken_, CSCSegmentCollH);
      if(!CSCSegmentCollH.isValid()){LogError("Analyzer") << "CSC Segment Collection not found!"; return;}

      iEvent.getByToken(muonDtSegmentToken_, DTSegmentCollH);
      if(!DTSegmentCollH.isValid()){LogError("Analyzer") << "DT Segment Collection not found!"; return;}
   }

   //reinitialize the bookeeping array for each event
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  HSCPTk        [CutIndex] = false;   }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  HSCPTk_SystP  [CutIndex] = false;   }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  HSCPTk_SystI  [CutIndex] = false;   }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  HSCPTk_SystT  [CutIndex] = false;   }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  HSCPTk_SystM  [CutIndex] = false;   }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  HSCPTk_SystPU [CutIndex] = false; }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  HSCPTk_SystHUp[CutIndex] = false; }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  HSCPTk_SystHDown[CutIndex] = false; }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  MaxMass       [CutIndex] = -1; }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  MaxMass_SystP [CutIndex] = -1; }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  MaxMass_SystI [CutIndex] = -1; }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  MaxMass_SystT [CutIndex] = -1; }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  MaxMass_SystM [CutIndex] = -1; }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  MaxMass_SystPU[CutIndex] = -1; }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  MaxMass_SystHUp [CutIndex] = -1; }
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){  MaxMass_SystHDown[CutIndex] = -1; }

   //WAIT//HIPemulator.setEventRate(); //take it from a pdf
   //WAIT//HIPemulatorUp.setEventRate(HIPemulator.getEventRatePixel()*1.25, HIPemulator.getEventRateStrip()*1.80);  // deltaPixel = 3.653981e+02, basePixel = 1.332625e+03; deltaStrip = 4.662832e+02, baseStrip = 5.958308e+02, from Run257805
   //WAIT//HIPemulatorDown.setEventRate(HIPemulator.getEventRatePixel()*0.75, HIPemulator.getEventRateStrip()*0.20); 

	//WAIT//HIPTrackLossEmul.SetHIPTrackLossRate(iEvent);

   

   //load all event collection that will be used later on (HSCP, dEdx and TOF)
   //====================loop over HSCP candidates===================
   unsigned int count = 0;
   for(const auto& hscp : iEvent.get(hscpToken_)){
      reco::MuonRef  muon  = hscp.muonRef();//const reco::MuonRef& muon = hscp.muonRef();

      //For TOF only analysis use updated stand alone muon track.
	   //Otherwise use inner tracker track
	   reco::TrackRef track;
      if(TypeMode_!=3) track = hscp.trackRef();
      else {
		   if(muon.isNull()) continue;
		   track = muon->standAloneMuon();
      }
      //skip events without track
	   if(track.isNull())continue;
      // FIXME jozze skip events with |Eta| > 0.9 (out of the barrel)
	   //if(track->eta()>0.9 || track->eta() < -0.9) continue;

      //require a track segment in the muon system
      if(TypeMode_>1 && TypeMode_!=5 && (muon.isNull() || !muon->isStandAloneMuon()))continue;

      //Apply a scale factor to muon only analysis to account for differences seen in data/MC preselection efficiency
      //For eta regions where Data > MC no correction to be conservative
      if(!isData && TypeMode_==3 && scaleFactor(track->eta())<RNG->Uniform(0, 1)) continue;

      //for signal only, make sure that the candidate is associated to a true HSCP
      int ClosestGen;
      if(isSignal && DistToHSCP(hscp, genColl, ClosestGen, TypeMode_)>0.03)continue;

      // we are losing some tracks due to HIP
	   //WAIT//if(!isData && is2016 && !HIPTrackLossEmul.TrackSurvivesHIPInefficiency()) continue;

      //load quantity associated to this track (TOF and dEdx)
      const reco::DeDxHitInfo* dedxHits = nullptr;
      if(TypeMode_!=3 && !track.isNull()) {
         reco::DeDxHitInfoRef dedxHitsRef = dedxCollH->get(track.key());
         if(!dedxHitsRef.isNull())dedxHits = &(*dedxHitsRef);
      }
      
      if(TypeMode_>1 && TypeMode_!=5 && !hscp.muonRef().isNull()){
         if(isMC){
            //WAIT//
            /*tof    = &tofMap[hscp.muonRef()];
            dttof  = &tofDtMap[hscp.muonRef()];
            csctof = &tofCscMap[hscp.muonRef()];*/
            tof    = &(*tofMap)[hscp.muonRef()];
            dttof  = &(*tofDtMap)[hscp.muonRef()];
            csctof = &(*tofCscMap)[hscp.muonRef()];
         }else{
            const CSCSegmentCollection& CSCSegmentColl = *CSCSegmentCollH;
            const DTRecSegment4DCollection& DTSegmentColl = *DTSegmentCollH;
            //std::cout<<"TESTA\n";
            tofCalculator.computeTOF(muon, CSCSegmentColl, DTSegmentColl, isData?1:0 ); //apply T0 correction on data but not on signal MC
            //std::cout<<"TESTB\n";
            tof    = &tofCalculator.combinedTOF; 
            dttof  = &tofCalculator.dtTOF;  
            csctof = &tofCalculator.cscTOF;
            //std::cout<<"TESTC\n";
         }
      }

      if(!dedxHits) continue; // skip tracks without hits otherwise there will be a crash

      HitDeDxCollection hitDeDx = getHitDeDx(dedxHits, dEdxSF, trackerCorrector.TrackerGains, false, 1);

      unsigned int pdgId = 0;
      if(isSignal){ 
         pdgId = genColl[ClosestGen].pdgId();
         LogDebug("Analyzer") << "GenId  " << pdgId;
      }

      double dEdxErr = 0;
      reco::DeDxData dedxSObjTmp  = computedEdx(dedxHits, dEdxSF, dEdxTemplates, true, useClusterCleaning, TypeMode_==5, false, trackerCorrector.TrackerGains, true, true, 99, false, 1, 0.00, nullptr,0,pdgId,skipPixel,useTemplateLayer);
      reco::DeDxData dedxMObjTmp = computedEdx(dedxHits, dEdxSF, nullptr,          true, useClusterCleaning, false      , false, trackerCorrector.TrackerGains, true, true, 99, false, 1, 0.15, nullptr, &dEdxErr,pdgId,skipPixel,useTemplateLayer);
      reco::DeDxData dedxMUpObjTmp = computedEdx(dedxHits, dEdxSF, nullptr,          true, useClusterCleaning, false      , false, trackerCorrector.TrackerGains, true, true, 99, false, 1, 0.15, nullptr,0,pdgId,skipPixel,useTemplateLayer);
      reco::DeDxData dedxMDownObjTmp = computedEdx(dedxHits, dEdxSF, nullptr,          true, useClusterCleaning, false      , false, trackerCorrector.TrackerGains, true, true, 99, false, 1, 0.15, nullptr,0,pdgId,skipPixel,useTemplateLayer);
      /*reco::DeDxData dedxMObjTmp = computedEdx(dedxHits, dEdxSF, nullptr,          true, useClusterCleaning, false      , false, trackerCorrector.TrackerGains, true, true, 99, false, 1, 0.15, (!isData && !is2016G)?&HIPemulator:nullptr, &dEdxErr,pdgId);
      reco::DeDxData dedxMUpObjTmp = computedEdx(dedxHits, dEdxSF, nullptr,          true, useClusterCleaning, false      , false, trackerCorrector.TrackerGains, true, true, 99, false, 1, 0.15, (!isData && !is2016G)?&HIPemulatorUp:nullptr,0,pdgId);
      reco::DeDxData dedxMDownObjTmp = computedEdx(dedxHits, dEdxSF, nullptr,          true, useClusterCleaning, false      , false, trackerCorrector.TrackerGains, true, true, 99, false, 1, 0.15, (!isData && !is2016G)?&HIPemulatorDown:nullptr,0,pdgId);*/
      reco::DeDxData* dedxSObj  = dedxSObjTmp.numberOfMeasurements()>0?&dedxSObjTmp:nullptr;
      reco::DeDxData* dedxMObj  = dedxMObjTmp.numberOfMeasurements()>0?&dedxMObjTmp:nullptr;
      reco::DeDxData* dedxMUpObj = dedxMUpObjTmp.numberOfMeasurements()>0?&dedxMUpObjTmp:nullptr;
      reco::DeDxData* dedxMDownObj = dedxMDownObjTmp.numberOfMeasurements()>0?&dedxMDownObjTmp:nullptr;
      if(TypeMode_==5)OpenAngle = deltaROpositeTrack(iEvent.get(hscpToken_), hscp); //OpenAngle is a global variable... that's uggly C++, but that's the best I found so far

      //compute systematic uncertainties on signal
      if(isSignal){
         //FIXME to be measured on 2015 data, currently assume 2012
         bool   PRescale = true;
         /*double IRescale =-0.05; // added to the Ias value
         double MRescale = 0.95;
		   double TRescale =-0.015; //-0.005 (used in 2012); // added to the 1/beta value*/
		  
		   double genpT = -1.0;
		   for(unsigned int g=0;g<genColl.size();g++) {
            if(genColl[g].pt()<5)continue;
            if(genColl[g].status()!=1)continue;
            int AbsPdg=abs(genColl[g].pdgId());
            if(AbsPdg!=17)continue;
            
            double separation = deltaR(track->eta(), track->phi(), genColl[g].eta(), genColl[g].phi());
            if (separation > 0.03) continue;
            genpT = genColl[g].pt();
            break;
         }
         if (genpT>0) {  tuple->genrecopT->Fill(genpT, track->pt()); }

         // compute systematic due to momentum scale
         //WAIT//if(PassPreselection( hscp,  dedxHits, dedxSObj, dedxMObj, tof, dttof, csctof, ev,  NULL, -1,   PRescale, 0, 0)){..}
         if(passPreselection( hscp,  dedxHits, dedxSObj, dedxMObj, tof, iEvent, EventWeight_,  tuple, -1,   PRescale, 0, 0, 0)){//WAIT//
            double RescalingFactor = RescaledPt(track->pt(),track->eta(),track->phi(),track->charge())/track->pt();

            if(TypeMode_==5 && isSemiCosmicSB)continue;
            double Mass     = -1; if(dedxMObj) Mass=GetMass(track->p()*RescalingFactor,dedxMObj->dEdx(), DeDxK, DeDxC);
            double MassTOF  = -1; if(tof)MassTOF = GetTOFMass(track->p()*RescalingFactor,tof->inverseBeta());
            double MassComb = -1;
		      if(tof && dedxMObj)MassComb=GetMassFromBeta(track->p()*RescalingFactor, (GetIBeta(dedxMObj->dEdx(), DeDxK, DeDxC) + (1/tof->inverseBeta()))*0.5);
		      else if(dedxMObj) MassComb = Mass;
		      if(tof) MassComb=MassTOF;

            for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){
		         if(passSelection(hscp,  dedxSObj, dedxMObj, tof, iEvent, EventWeight_, CutIndex, tuple, false, -1,   PRescale, 0, 0)){//WAIT//
                  HSCPTk_SystP[CutIndex] = true;
                  if(Mass>MaxMass_SystP[CutIndex]) MaxMass_SystP[CutIndex]=Mass;
                  tuple->Mass_SystP->Fill(CutIndex, Mass,EventWeight_);
                  if(tof)
                     {tuple->MassTOF_SystP ->Fill(CutIndex, MassTOF , EventWeight_);}
                     tuple->MassComb_SystP->Fill(CutIndex, MassComb, EventWeight_);
               }
            }
         }

         // compute systematic due to dEdx (both Ias and Ih)
         //WAIT//if(PassPreselection( hscp,  dedxHits, dedxSObj, dedxMObj, tof, dttof, csctof, ev,  NULL, -1,   0, IRescale, 0)){...}

         // compute systematic due to Mass shift
         //WAIT//if(PassPreselection( hscp,  dedxHits, dedxSObj, dedxMObj, tof, dttof, csctof, ev,  NULL, -1,   0, 0, 0)){...}

         // compute systematic due to TOF
         //WAIT//if(PassPreselection( hscp,  dedxHits, dedxSObj, dedxMObj, tof, dttof, csctof, ev,  NULL, -1,   0, 0, TRescale)){...}

         // compute systematics due to PU

      }//End of systematic computation for signal

      //check if the canddiate pass the preselection cuts
      /*const susybsm::HSCParticle& hscp, const DeDxHitInfo* dedxHits,  const reco::DeDxData* dedxSObj, const reco::DeDxData* dedxMObj, const reco::MuonTimeExtra* tof, const reco::MuonTimeExtra* dttof, const reco::MuonTimeExtra* csctof, const ChainEvent& ev, stPlots* st, const double& GenBeta, bool RescaleP, const double& RescaleI, const double& RescaleT, double MassErr*/
      double MassErr = GetMassErr(track->p(), track->ptError(), dedxMObj?dedxMObj->dEdx():-1, dEdxErr, GetMass(track->p(), dedxMObj?dedxMObj->dEdx():-1, DeDxK,DeDxC), DeDxK,DeDxC);
      if(isMC){
         passPreselection(  hscp, dedxHits, dedxSObj, dedxMObj, tof, iEvent, EventWeight_, tuple, -1, false, 0, 0, MassErr );
      }
      if(!passPreselection( hscp, dedxHits, dedxSObj, dedxMObj, tof, iEvent, EventWeight_, tuple, isSignal?genColl[ClosestGen].p()/genColl[ClosestGen].energy():-1, false, 0, 0, MassErr) ) continue;
      if(TypeMode_==5 && isSemiCosmicSB)continue;//WAIT//

      //fill the ABCD histograms and a few other control plots
      //WAIT//if(isData)Analysis_FillControlAndPredictionHist(hscp, dedxSObj, dedxMObj, tof, SamplePlots);
      //WAIT//else if(isMC) Analysis_FillControlAndPredictionHist(hscp, dedxSObj, dedxMObj, tof, MCTrPlots);

      tuple_maker->fillControlAndPredictionHist(hscp, dedxSObj, dedxMObj, tof, tuple, TypeMode_, GlobalMinTOF, EventWeight_,isCosmicSB, DTRegion, MaxPredBins, isMCglobal, DeDxK, DeDxC, CutPt_, CutI_, CutTOF_, CutPt_Flip_, CutI_Flip_, CutTOF_Flip_);

      if(TypeMode_==5 && isCosmicSB)continue; 

      //Find the number of tracks passing selection for TOF<1 that will be used to check the background prediction
      //double Mass = -1;
      if(isMC || isData) {
         //compute the mass of the candidate, for TOF mass flip the TOF over 1 to get the mass, so 0.8->1.2
		   double Mass = GetMass(track->p(),dedxMObj->dEdx(),DeDxK,DeDxC);
		   double MassTOF  = -1; if(tof) MassTOF = GetTOFMass(track->p(),(2-tof->inverseBeta()));
		   double MassComb = -1;
		   if(tof && dedxMObj)MassComb=GetMassFromBeta(track->p(), (GetIBeta(dedxMObj->dEdx(),DeDxK,DeDxC) + (1/(2-tof->inverseBeta())))*0.5 ) ;
		   if(dedxMObj) MassComb = Mass;
		   if(tof) MassComb=GetMassFromBeta(track->p(),(1/(2-tof->inverseBeta())));

         for(unsigned int CutIndex=0;CutIndex<CutPt_Flip_.size();CutIndex++){
            //Background check looking at region with TOF<1
            //WAIT//if(!PassSelection   (hscp, dedxSObj, dedxMObj, tof, ev, CutIndex, NULL, true)) continue;

            //Fill Mass Histograms
            tuple->Mass_Flip->Fill(CutIndex, Mass,EventWeight_);
            if(tof) tuple->MassTOF_Flip->Fill(CutIndex, MassTOF, EventWeight_);
            tuple->MassComb_Flip->Fill(CutIndex, MassComb, EventWeight_);
		   }

      }

      //compute the mass of the candidate
      double Mass     = -1; if(dedxMObj) Mass = GetMass(track->p(),dedxMObj->dEdx(),DeDxK,DeDxC);
      double MassTOF  = -1; if(tof) MassTOF = GetTOFMass(track->p(),tof->inverseBeta());
      double MassComb = -1;
      if(tof && dedxMObj) MassComb=GetMassFromBeta(track->p(), (GetIBeta(dedxMObj->dEdx(),DeDxK,DeDxC) + (1/tof->inverseBeta()))*0.5 ) ;
      if(dedxMObj) MassComb = Mass;
      if(tof) MassComb=GetMassFromBeta(track->p(),(1/tof->inverseBeta()));

      double MassUp    = -1; if(dedxMUpObj) MassUp=GetMass(track->p(),dedxMUpObj->dEdx(),DeDxK,DeDxC);
      double MassUpComb = -1;
      if(tof && dedxMUpObj) MassUpComb=GetMassFromBeta(track->p(), (GetIBeta(dedxMUpObj->dEdx(),DeDxK,DeDxC) + (1/tof->inverseBeta()))*0.5 ) ;
      if(dedxMUpObj) MassUpComb = MassUp;
      if(tof) MassUpComb=GetMassFromBeta(track->p(),(1/tof->inverseBeta()));

      double MassDown    = -1; if(dedxMDownObj) MassDown=GetMass(track->p(),dedxMDownObj->dEdx(),DeDxK,DeDxC);
      double MassDownComb = -1;
      if(tof && dedxMDownObj) MassDownComb=GetMassFromBeta(track->p(), (GetIBeta(dedxMDownObj->dEdx(),DeDxK,DeDxC) + (1/tof->inverseBeta()))*0.5 ) ;
      if(dedxMDownObj) MassDownComb = MassDown;
      if(tof) MassDownComb=GetMassFromBeta(track->p(),(1/tof->inverseBeta()));

      bool PassNonTrivialSelection=false;

      //==========================================================
      // Cut loop: over all possible selection (one of them, the optimal one, will be used later)
      for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){
         //Full Selection
         //if(isMC)passSelection   (hscp, dedxSObj, dedxMObj, tof, ev, CutIndex, MCTrPlots);
         if(isMC) passSelection(hscp, dedxSObj, dedxMObj, tof, iEvent, EventWeight_, CutIndex, tuple, false, -1, false, 0, 0);
         if(     !passSelection(hscp, dedxSObj, dedxMObj, tof, iEvent, EventWeight_, CutIndex, tuple, false, isSignal?genColl[ClosestGen].p()/genColl[ClosestGen].energy():-1, false, 0, 0) ) continue;

         if(CutIndex!=0)PassNonTrivialSelection=true;
         HSCPTk[CutIndex] = true;
         HSCPTk_SystHUp[CutIndex] = true;
         HSCPTk_SystHDown[CutIndex] = true;

         if(Mass>MaxMass[CutIndex]) MaxMass[CutIndex]=Mass;
         if(MassUp>MaxMass_SystHUp[CutIndex]) MaxMass_SystHUp[CutIndex]=Mass;
         if(MassDown>MaxMass_SystHDown[CutIndex]) MaxMass_SystHDown[CutIndex]=Mass;

         //Fill Mass Histograms
         tuple->Mass->Fill(CutIndex, Mass,EventWeight_);
         if(tof) tuple->MassTOF->Fill(CutIndex, MassTOF, EventWeight_);
         if(isMC) tuple->MassComb->Fill(CutIndex, MassComb, EventWeight_);

         //Fill Mass Histograms for different Ih syst
         tuple->Mass_SystHUp  ->Fill(CutIndex, MassUp,EventWeight_);
         tuple->Mass_SystHDown->Fill(CutIndex, MassDown,EventWeight_);
         if(tof) tuple->MassTOF_SystH ->Fill(CutIndex, MassTOF, EventWeight_);
         tuple->MassComb_SystHUp  ->Fill(CutIndex, MassUpComb, EventWeight_);
         tuple->MassComb_SystHDown->Fill(CutIndex, MassDownComb, EventWeight_);

      }//end of Cut loop

      double Ick2=0;  if(dedxMObj) Ick2=GetIck(dedxMObj->dEdx(),isMC,DeDxK,DeDxC);
      int nomh= 0;nomh = track->hitPattern().trackerLayersWithoutMeasurement(reco::HitPattern::MISSING_INNER_HITS) + track->hitPattern().trackerLayersWithoutMeasurement(reco::HitPattern::TRACK_HITS);
      double fovhd = track->found()<=0?-1:track->found() / float(track->found() + nomh);
      unsigned int nom=0; if(dedxSObj) nom=dedxSObj->numberOfMeasurements();

      double weight=0,genid=0,gencharge=-99,genmass=-99,genpt=-99,geneta=-99,genphi=-99;
      weight = EventWeight_;
  
      if(isSignal){
         genid = genColl[ClosestGen].pdgId();
         gencharge = genColl[ClosestGen].charge();
         genmass = genColl[ClosestGen].mass();
         genpt = genColl[ClosestGen].pt();
         geneta = genColl[ClosestGen].eta();
         genphi = genColl[ClosestGen].phi();
      }

      if(PassNonTrivialSelection||(dedxSObj && dedxSObj->dEdx()> 0. && track->pt()>60.))
         tuple_maker->fillTreeBranches(tuple,
            TrigInfo_, iEvent.id().run(),iEvent.id().event(),iEvent.id().luminosityBlock(), 
            count, track->charge(), track->pt(),track->ptError(), 
            dedxSObj ? dedxSObj->dEdx() : -1,
            dedxSObj ? dedxMObj->dEdx() : -1,
            dedxMObj ? Ick2 : -99, 
            tof ? tof->inverseBeta() : -1, 
            Mass, TreeDZ, TreeDXY, OpenAngle, 
            track->eta(), track->phi(), track->found(), track->hitPattern().numberOfValidPixelHits(), track->validFraction(), 
            nomh,fovhd, nom, weight,genid,gencharge,genmass,genpt,geneta,genphi
         );
   count++;
   } //END loop over HSCP candidates

   //save event dependent information thanks to the bookkeeping
   for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){
      if(HSCPTk[CutIndex]){
         tuple->HSCPE->Fill(CutIndex,EventWeight_);
         tuple->MaxEventMass->Fill(CutIndex,MaxMass[CutIndex], EventWeight_);
         if(isMC){
            tuple->HSCPE->Fill(CutIndex,EventWeight_);
            tuple->MaxEventMass->Fill(CutIndex,MaxMass[CutIndex], EventWeight_);
         }
      }
      if(HSCPTk_SystP[CutIndex]){
         tuple->HSCPE_SystP       ->Fill(CutIndex,EventWeight_);
         tuple->MaxEventMass_SystP->Fill(CutIndex,MaxMass_SystP[CutIndex], EventWeight_);
      }
      if(HSCPTk_SystI[CutIndex]){
         tuple->HSCPE_SystI       ->Fill(CutIndex,EventWeight_);
         tuple->MaxEventMass_SystI->Fill(CutIndex,MaxMass_SystI[CutIndex], EventWeight_);
      }
      if(HSCPTk_SystM[CutIndex]){
         tuple->HSCPE_SystM       ->Fill(CutIndex,EventWeight_);
         tuple->MaxEventMass_SystM->Fill(CutIndex,MaxMass_SystM[CutIndex], EventWeight_);
      }
      if(HSCPTk_SystT[CutIndex]){
         tuple->HSCPE_SystT       ->Fill(CutIndex,EventWeight_);
         tuple->MaxEventMass_SystT->Fill(CutIndex,MaxMass_SystT[CutIndex], EventWeight_);
      }
      if(HSCPTk_SystPU[CutIndex]){
         tuple->HSCPE_SystPU       ->Fill(CutIndex,EventWeight_*PUSystFactor_);
         tuple->MaxEventMass_SystPU->Fill(CutIndex,MaxMass_SystPU[CutIndex], EventWeight_*PUSystFactor_);
      }
      if(HSCPTk_SystHUp[CutIndex]){
         tuple->HSCPE_SystHUp     ->Fill(CutIndex,EventWeight_);
         tuple->MaxEventMass_SystHUp   ->Fill(CutIndex,MaxMass_SystHUp   [CutIndex], EventWeight_);
      }
      if(HSCPTk_SystHDown[CutIndex]){
         tuple->HSCPE_SystHDown   ->Fill(CutIndex,EventWeight_);
         tuple->MaxEventMass_SystHDown ->Fill(CutIndex,MaxMass_SystHDown [CutIndex], EventWeight_);
      }
   }

#ifdef THIS_IS_AN_EVENTSETUP_EXAMPLE
   ESHandle<SetupData> pSetup;
   iSetup.get<SetupRecord>().get(pSetup);
#endif
}

// ------------ method called once each job just after ending the event loop  ------------
void
Analyzer::endJob()
{
   delete RNG;
   delete [] HSCPTk;
   delete [] HSCPTk_SystP;
   delete [] HSCPTk_SystI;
   delete [] HSCPTk_SystT;
   delete [] HSCPTk_SystM;
   delete [] HSCPTk_SystPU;
   delete [] HSCPTk_SystHUp;
   delete [] HSCPTk_SystHDown;
   delete [] MaxMass;
   delete [] MaxMass_SystP;
   delete [] MaxMass_SystI;
   delete [] MaxMass_SystT;
   delete [] MaxMass_SystM;
   delete [] MaxMass_SystPU;
   delete [] MaxMass_SystHUp;
   delete [] MaxMass_SystHDown;
}

// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void
Analyzer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  //The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);

  //Specify that only 'tracks' is allowed
  //To use, remove the default given above and uncomment below
  //ParameterSetDescription desc;
  //desc.addUntracked<edm::InputTag>("tracks","ctfWithMaterialTracks");
  //descriptions.addDefault(desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(Analyzer);

//=============================================================
//
//     Method for initializing pT and Is cuts
//
//=============================================================
void Analyzer::initializeCuts(edm::Service<TFileService> &fs, vector<double>  &CutPt, vector<double>  &CutI, vector<double>  &CutTOF, vector<double>  &CutPt_Flip, vector<double>  &CutI_Flip, vector<double>  &CutTOF_Flip){
   CutPt.clear();       CutI.clear();       CutTOF.clear();      
   CutPt_Flip.clear();  CutI_Flip.clear();  CutTOF_Flip.clear();  
   
   CutPt     .push_back(GlobalMinPt);   CutI       .push_back(GlobalMinIs);  CutTOF     .push_back(GlobalMinTOF);
   CutPt_Flip.push_back(GlobalMinPt);   CutI_Flip  .push_back(GlobalMinIs);  CutTOF_Flip.push_back(GlobalMinTOF);

   if(TypeMode_<2){   
      for(double Pt =GlobalMinPt+5 ; Pt <200;Pt+=5){
         for(double I  =GlobalMinIs+0.025  ; I  <0.45 ;I+=0.025){
            CutPt .push_back(Pt);   CutI  .push_back(I);  CutTOF.push_back(-1);
         }
      }
   }else if(TypeMode_==2){
      for(double Pt =GlobalMinPt+5 ; Pt <120;  Pt+=5){
      if(Pt>80 && ((int)Pt)%10!=0)continue;
      for(double I  =GlobalMinIs +0.025; I  <0.40;  I+=0.025){
      for(double TOF=GlobalMinTOF+0.025; TOF<1.35;TOF+=0.025){
	   CutPt .push_back(Pt);   CutI  .push_back(I);  CutTOF.push_back(TOF);
      }}}
      for(double Pt =GlobalMinPt+10 ; Pt <90;  Pt+=30){
      for(double I  =GlobalMinIs +0.1; I  <0.30;  I+=0.1){
      for(double TOF=GlobalMinTOF-0.05; TOF>0.65;TOF-=0.05){
	   CutPt_Flip .push_back(Pt);   CutI_Flip  .push_back(I);  CutTOF_Flip.push_back(TOF);
      }}}
   }else if(TypeMode_==3){
      for(double Pt =GlobalMinPt+30 ; Pt <450;  Pt+=30){
      for(double TOF=GlobalMinTOF+0.025; TOF<1.5;TOF+=0.025){
         CutPt .push_back(Pt);   CutI  .push_back(-1);  CutTOF.push_back(TOF);
      }}
      for(double Pt =GlobalMinPt+30 ; Pt <450;  Pt+=60){
      for(double TOF=GlobalMinTOF-0.025; TOF>0.5;TOF-=0.025){
      CutPt_Flip .push_back(Pt);   CutI_Flip  .push_back(-1);  CutTOF_Flip.push_back(TOF);
      }}
   }else if(TypeMode_==4){
      for(double I  =GlobalMinIs +0.025; I  <0.55;  I+=0.025){
      for(double TOF=GlobalMinTOF+0.025; TOF<1.46;TOF+=0.025){
 	   CutPt .push_back(-1);   CutI  .push_back(I);  CutTOF.push_back(TOF);
       }}
      for(double I  =GlobalMinIs +0.025; I  <0.55;  I+=0.025){
      for(double TOF=GlobalMinTOF-0.025; TOF>0.54;TOF-=0.025){
	 CutPt_Flip .push_back(-1);   CutI_Flip  .push_back(I);  CutTOF_Flip.push_back(TOF);
       }}
   }else if(TypeMode_==5){   
      for(double Pt =75 ; Pt <=150;Pt+=25){
      for(double I  =0.0; I  <=0.45 ;I+=0.025){
         CutPt     .push_back(Pt);   CutI     .push_back(I);  CutTOF     .push_back(-1);
         CutPt_Flip.push_back(Pt);   CutI_Flip.push_back(I);  CutTOF_Flip.push_back(-1);
     }}
   }

   //printf("%i Different Final Selection will be tested\n",(int)CutPt.size());
   //printf("%i Different Final Selection will be tested for background uncertainty\n",(int)CutPt_Flip.size());
   edm::LogInfo("Analyzer") << CutPt.size() << " Different Final Selection will be tested\n"
                       << CutPt_Flip.size() << " Different Final Selection will be tested for background uncertainty";

   //Initialization of variables that are common to all samples
   HCuts["Pt"]  = fs->make<TProfile>("HCuts_Pt" ,"HCuts_Pt" ,CutPt.size(),0,CutPt.size());
   HCuts["I"]   = fs->make<TProfile>("HCuts_I"  ,"HCuts_I"  ,CutPt.size(),0,CutPt.size());
   HCuts["TOF"] = fs->make<TProfile>("HCuts_TOF","HCuts_TOF",CutPt.size(),0,CutPt.size());
   for(unsigned int i=0;i<CutPt.size();i++){  
      HCuts["Pt"]->Fill(i,CutPt[i]);     
      HCuts["I"]->Fill(i,CutI[i]);    
      HCuts["TOF"]->Fill(i,CutTOF[i]); 
   }

   HCuts["Pt_Flip"]  = fs->make<TProfile>("HCuts_Pt_Flip" ,"HCuts_Pt_Flip" ,CutPt_Flip.size(),0,CutPt_Flip.size());
   HCuts["I_Flip"]   = fs->make<TProfile>("HCuts_I_Flip"  ,"HCuts_I_Flip"  ,CutPt_Flip.size(),0,CutPt_Flip.size());
   HCuts["TOF_Flip"] = fs->make<TProfile>("HCuts_TOF_Flip","HCuts_TOF_Flip",CutPt_Flip.size(),0,CutPt_Flip.size());
   for(unsigned int i=0;i<CutPt_Flip.size();i++){  
      HCuts["Pt_Flip"]->Fill(i,CutPt_Flip[i]);     
      HCuts["I_Flip"]->Fill(i,CutI_Flip[i]);    
      HCuts["TOF_Flip"]->Fill(i,CutTOF_Flip[i]); 
   }
}

//=============================================================
//
//     Method for scaling eta per bin
//
//=============================================================
double Analyzer::scaleFactor(double eta) {
  double etaBins[15]   = {-2.1, -1.8, -1.5, -1.2, -0.9, -0.6, -0.3, 0.0 , 0.3 , 0.6 , 0.9 , 1.2 ,1.5 , 1.8 , 2.1 };
  double scaleBins[15] = {0,    0.97, 1.06, 1.00, 0.89, 0.91, 0.93, 0.93, 0.92, 0.92, 0.91, 0.89,1.00, 1.06, 0.99};
  for (int i=0; i<15; i++) if(eta<etaBins[i]) return scaleBins[i];
  return 0;
}

//=============================================================
//
//     Method for rescaling pT
//
//=============================================================
double Analyzer::RescaledPt(const double& pt, const double& eta, const double& phi, const int& charge){
  if(TypeMode_!=3) {
    double newInvPt = 1/pt+0.000236-0.000135*pow(eta,2)+charge*0.000282*TMath::Sin(phi-1.337);
    return 1/newInvPt;
  }
  else {
    double newInvPt = (1./pt)*1.1;
    return 1/newInvPt;
  }
}

//=============================================================
//
//     Method to get hit position
//
//=============================================================
TVector3 Analyzer::getOuterHitPos(const reco::DeDxHitInfo* dedxHits){
     TVector3 point(0,0,0);
     if(!dedxHits)return point;
     double outerDistance=-1;
     for(unsigned int h=0;h<dedxHits->size();h++){
        DetId detid(dedxHits->detId(h));  
        moduleGeom* geomDet = moduleGeom::get(detid.rawId());
        //WAIT//TVector3 hitPos = geomDet->toGlobal(TVector3(dedxHits->pos(h).x(), dedxHits->pos(h).y(), dedxHits->pos(h).z())); 
        //WAIT//if(hitPos.Mag()>outerDistance){outerDistance=hitPos.Mag();  point=hitPos;}
     }
     return point;
}

//=============================================================
//
//     Method for ...
//
//=============================================================
double Analyzer::SegSep(const susybsm::HSCParticle& hscp, const edm::Event& iEvent, double& minPhi, double& minEta){
  if(TypeMode_!=3)return -1;

  reco::MuonRef muon = hscp.muonRef();
  if(muon.isNull()) return false;
  reco::TrackRef  track = muon->standAloneMuon();
  if(track.isNull())return false;


  double minDr=10;
  minPhi=10;
  minEta=10;

  //Look for segment on opposite side of detector from track
  //susybsm::MuonSegmentCollection SegCollection = iEvent.get(muonSegmentToken_);
  //for (susybsm::MuonSegmentCollection::const_iterator segment = SegCollection.begin(); segment!=SegCollection.end();++segment) {  
  for( const auto& segment : iEvent.get(muonSegmentToken_) ){
    GlobalPoint gp = segment.getGP();

    //Flip HSCP to opposite side of detector
    double eta_hscp = -1*track->eta();
    double phi_hscp= track->phi()+M_PI;

    double deta = gp.eta() - eta_hscp;
    double dphi = gp.phi() - phi_hscp;
    while (dphi >   M_PI) dphi -= 2*M_PI;
    while (dphi <= -M_PI) dphi += 2*M_PI;

    //Find segment most opposite in eta
    //Require phi difference of 0.5 so it doesn't match to own segment
    if(fabs(deta)<fabs(minEta) && fabs(dphi)<(M_PI-0.5)) {
      minEta=deta;
    }
    //Find segment most opposite in phi
    if(fabs(dphi)<fabs(minPhi)) {
      minPhi=dphi;
    }
    //Find segment most opposite in Eta-Phi
    double dR=sqrt(deta*deta+dphi*dphi);
    if(dR<minDr) minDr=dR;
  }
  return minDr;
}

//=============================================================
//
//     Pre-Selection
//
//=============================================================
bool Analyzer::passPreselection(
   const susybsm::HSCParticle& hscp, 
   const reco::DeDxHitInfo* dedxHits,  
   const reco::DeDxData* dedxSObj, 
   const reco::DeDxData* dedxMObj,
   const reco::MuonTimeExtra* tof,
   const edm::Event& iEvent, 
   float Event_Weight,
   Tuple* &tuple, 
   const double& GenBeta, 
   bool RescaleP, 
   const double& RescaleI, 
   const double& RescaleT, 
   double MassErr)
{

   if(TypeMode_==1 && !(hscp.type() == susybsm::HSCParticleType::trackerMuon || hscp.type() == susybsm::HSCParticleType::globalMuon))return false;
   if( (TypeMode_==2 || TypeMode_==4) && hscp.type() != susybsm::HSCParticleType::globalMuon)return false;

   reco::TrackRef   track;
   reco::MuonRef muon = hscp.muonRef();

   if(TypeMode_!=3) track = hscp.trackRef();
   else {
     if(muon.isNull()) return false;
     track = muon->standAloneMuon();
   }
   if(track.isNull())return false;

   if(tuple){tuple->Total->Fill(0.0,Event_Weight);
     if(GenBeta>=0)tuple->Beta_Matched->Fill(GenBeta, Event_Weight);
     tuple->BS_Eta->Fill(track->eta(),Event_Weight);
   }

   if(fabs(track->eta())>GlobalMaxEta) return false;

   //Cut on number of matched muon stations
   int count = muonStations(track->hitPattern());
   if(tuple) {
     tuple->BS_MatchedStations->Fill(count, Event_Weight);
   }
   if(TypeMode_==3 && count<minMuStations) return false;
   if(tuple) tuple->Stations->Fill(0.0, Event_Weight);

   //===================== Handle For vertex ================
   vector<reco::Vertex> vertexColl = iEvent.get(offlinePrimaryVerticesToken_);
   if(vertexColl.size()<1){edm::LogError("Analyzer") << "NO VERTEX"; return false;}

   int highestPtGoodVertex = -1;
   int goodVerts=0;
   double dzMin=10000;
   for(unsigned int i=0;i<vertexColl.size();i++){
      if(vertexColl[i].isFake() || fabs(vertexColl[i].z())>24 || vertexColl[i].position().rho()>2 || vertexColl[i].ndof()<=4)continue; //only consider good vertex
      goodVerts++;
      if(tuple) tuple->BS_dzAll->Fill( track->dz (vertexColl[i].position()),Event_Weight);
      if(tuple) tuple->BS_dxyAll->Fill(track->dxy(vertexColl[i].position()),Event_Weight);

      if(fabs(track->dz (vertexColl[i].position())) < fabs(dzMin) ){
         dzMin = fabs(track->dz (vertexColl[i].position()));
         highestPtGoodVertex = i;
      }
   }
   if(highestPtGoodVertex<0)highestPtGoodVertex=0;

   if(tuple){tuple->BS_NVertex->Fill(vertexColl.size(), Event_Weight);
     tuple->BS_NVertex_NoEventWeight->Fill(vertexColl.size());
   }
   double dz  = track->dz (vertexColl[highestPtGoodVertex].position());
   double dxy = track->dxy(vertexColl[highestPtGoodVertex].position());

   bool PUA = (vertexColl.size()<15);
   bool PUB = (vertexColl.size()>=15);

   if(tuple){tuple->BS_TNOH->Fill(track->found(),Event_Weight);
          if(PUA)tuple->BS_TNOH_PUA->Fill(track->found(),Event_Weight);
          if(PUB)tuple->BS_TNOH_PUB->Fill(track->found(),Event_Weight);
          tuple->BS_TNOHFraction->Fill(track->validFraction(),Event_Weight);
	  tuple->BS_TNOPH->Fill(track->hitPattern().numberOfValidPixelHits(),Event_Weight);
   }

   if(TypeMode_!=3 && track->found()<GlobalMinNOH)return false;

   if(TypeMode_!=3 && track->hitPattern().numberOfValidPixelHits()<GlobalMinNOPH)return false;
   if(TypeMode_!=3 && track->validFraction()<GlobalMinFOVH)return false;

   unsigned int missingHitsTillLast = track->hitPattern().trackerLayersWithoutMeasurement(reco::HitPattern::MISSING_INNER_HITS) + track->hitPattern().trackerLayersWithoutMeasurement(reco::HitPattern::TRACK_HITS);;
   double validFractionTillLast = track->found()<=0?-1:track->found() / float(track->found() + missingHitsTillLast);
  
   if(tuple){
      tuple->BS_TNOHFractionTillLast->Fill(validFractionTillLast,Event_Weight);
	   tuple->BS_TNOMHTillLast->Fill(missingHitsTillLast,Event_Weight);
   }

   if(TypeMode_!=3 && missingHitsTillLast>GlobalMaxNOMHTillLast)return false;
   if(TypeMode_!=3 && validFractionTillLast<GlobalMinFOVHTillLast)return false;

   if(tuple){
      tuple->TNOH  ->Fill(0.0,Event_Weight);
      if(dedxSObj){
         tuple->BS_TNOM->Fill(dedxSObj->numberOfMeasurements(),Event_Weight);
         if(track->found() - dedxSObj->numberOfMeasurements()) 
             tuple->BS_EtaNBH->Fill(track->eta(), track->found() - dedxSObj->numberOfMeasurements(), Event_Weight);
         if(PUA)tuple->BS_TNOM_PUA->Fill(dedxSObj->numberOfMeasurements(),Event_Weight);
         if(PUB)tuple->BS_TNOM_PUB->Fill(dedxSObj->numberOfMeasurements(),Event_Weight);
      }
   }
   if(dedxSObj) if(dedxSObj->numberOfMeasurements()<GlobalMinNOM)return false;
   if(tuple){tuple->TNOM  ->Fill(0.0,Event_Weight);}

   if(tof){
      if(tuple){tuple->BS_nDof->Fill(tof->nDof(),Event_Weight);}
      if((TypeMode_>1  && TypeMode_!=5) && tof->nDof()<GlobalMinNDOF && (dttof->nDof()<GlobalMinNDOFDT || csctof->nDof()<GlobalMinNDOFCSC) )return false;
   }
   if(tuple){
      tuple->nDof  ->Fill(0.0,Event_Weight);
      tuple->BS_Qual->Fill(track->qualityMask(),Event_Weight);
   }

   if(TypeMode_!=3 && track->qualityMask()<GlobalMinQual )return false; // FIXME Tracks with quality > 2 are bad also!
//   if(TypeMode_!=3 && track->qualityMask() != FixedQual)return false; // FIXME if this is true, no tracks pass eventually ... so what now?
   if(tuple){tuple->Qual  ->Fill(0.0,Event_Weight);
          tuple->BS_Chi2->Fill(track->chi2()/track->ndof(),Event_Weight);
   }
   if(TypeMode_!=3 && track->chi2()/track->ndof()>GlobalMaxChi2 )return false;
   if(tuple){tuple->Chi2  ->Fill(0.0,Event_Weight);}

   if(tuple && GenBeta>=0)tuple->Beta_PreselectedA->Fill(GenBeta, Event_Weight);

   if(tuple){tuple->BS_MPt ->Fill(track->pt(),Event_Weight);}
   if(RescaleP){ if(RescaledPt(track->pt(),track->eta(),track->phi(),track->charge())<GlobalMinPt)return false;
   }else{        if(track->pt()<GlobalMinPt)return false;   }

   if(tuple){tuple->MPt   ->Fill(0.0,Event_Weight);
     if(dedxSObj) tuple->BS_MIs->Fill(dedxSObj->dEdx(),Event_Weight);
     if(dedxMObj) tuple->BS_MIm->Fill(dedxMObj->dEdx(),Event_Weight);
   }

   if(dedxSObj && dedxSObj->dEdx()+RescaleI<GlobalMinIs)return false;
   if(dedxMObj && ((TypeMode_!=5 && dedxMObj->dEdx()<GlobalMinIm) || (TypeMode_==5 && dedxMObj->dEdx()>GlobalMinIm)) )return false;
   if(tuple){tuple->MI   ->Fill(0.0,Event_Weight);}

   if(tof){
      if(tuple){tuple->BS_MTOF ->Fill(tof->inverseBeta(),Event_Weight);}
      //This cut is no longer applied here but rather in the PassSelection part to use the region
      //with TOF<GlobalMinTOF as a background check
      //if(TypeMode_>1 && tof->inverseBeta()+RescaleT<GlobalMinTOF)return false;

      if(tuple)tuple->BS_TOFError->Fill(tof->inverseBetaErr(),Event_Weight);
      if((TypeMode_>1  && TypeMode_!=5) && tof->inverseBetaErr()>GlobalMaxTOFErr)return false;

      if(tuple) tuple->BS_TimeAtIP->Fill(tof->timeAtIpInOut(),Event_Weight);
      if(TypeMode_==3 && min(min(fabs(tof->timeAtIpInOut()-100), fabs(tof->timeAtIpInOut()-50)), min(fabs(tof->timeAtIpInOut()+100), fabs(tof->timeAtIpInOut()+50)))<5) return false;
   }

   if(tuple) tuple->BS_dzMinv3d->Fill(dz,Event_Weight);
   if(tuple) tuple->BS_dxyMinv3d->Fill(dxy,Event_Weight);
   if(tuple) tuple->BS_PV->Fill(goodVerts,Event_Weight);   
   if(tuple) tuple->BS_PV_NoEventWeight->Fill(goodVerts);
   if(tuple && dedxSObj) tuple->BS_NOMoNOHvsPV->Fill(goodVerts,dedxSObj->numberOfMeasurements()/(double)track->found(),Event_Weight);

   //Require at least one good vertex except if cosmic event
   //WAIT//if(TypeMode_==3 && goodVerts<1 && (!tuple || tuple->Name.find("Cosmic")==string::npos)) return false;

   //For TOF only analysis match to a SA track without vertex constraint for IP cuts
   if(TypeMode_==3) {

      //Find closest NV track
      const std::vector<reco::Track> noVertexTrackColl = iEvent.get(refittedStandAloneMuonsToken_);
      reco::Track NVTrack;
      double minDr=15;
      for(unsigned int i=0;i<noVertexTrackColl.size();i++){
         double dR = deltaR(track->eta(), track->phi(), noVertexTrackColl[i].eta(), noVertexTrackColl[i].phi());
         if(dR<minDr) {
            minDr=dR;
	         NVTrack=noVertexTrackColl[i];
         }
      }
      if(tuple) tuple->BS_dR_NVTrack->Fill(minDr,Event_Weight);
      if(minDr>0.4) return false;
      if(tuple)tuple->NVTrack->Fill(0.0,Event_Weight);

      //Find displacement of tracks with respect to beam spot
      const reco::BeamSpot beamSpotColl = iEvent.get(offlineBeamSpotToken_);

      dz  = NVTrack.dz (beamSpotColl.position());
      dxy = NVTrack.dxy(beamSpotColl.position());
      if(muonStations(NVTrack.hitPattern())<minMuStations) return false;
   }

   if(tuple){tuple->MTOF ->Fill(0.0,Event_Weight);
     if(GenBeta>=0)tuple->Beta_PreselectedB->Fill(GenBeta, Event_Weight);
   }

   double v3d = sqrt(dz*dz+dxy*dxy);

   if(tuple){tuple->BS_V3D->Fill(v3d,Event_Weight);}
   if(v3d>GlobalMaxV3D )return false;
   if(tuple){tuple->V3D  ->Fill(0.0,Event_Weight);}

   if(tuple)tuple->BS_Dxy->Fill(dxy, Event_Weight);

   TreeDXY = dxy;   
   bool DXYSB = false;
   if(TypeMode_!=5 && fabs(dxy)>GlobalMaxDXY)return false;
   if(TypeMode_==5 && fabs(dxy)>4)return false;
   if(TypeMode_==5 && fabs(dxy)>GlobalMaxDXY) DXYSB = true;

   if(tuple){tuple->Dxy  ->Fill(0.0,Event_Weight);}

   if(TypeMode_!=3) {
     const edm::ValueMap<susybsm::HSCPIsolation> IsolationMap = iEvent.get(hscpIsoToken_);

     susybsm::HSCPIsolation hscpIso = IsolationMap.get((size_t)track.key());
     if(tuple){tuple->BS_TIsol ->Fill(hscpIso.Get_TK_SumEt(),Event_Weight);}
      //     if(TypeMode_!=4){       if(hscpIso.Get_TK_SumEt()>GlobalMaxTIsol)return false;     }
      if(hscpIso.Get_TK_SumEt()>GlobalMaxTIsol)return false;
     if(tuple){tuple->TIsol   ->Fill(0.0,Event_Weight);}

     double EoP = (hscpIso.Get_ECAL_Energy() + hscpIso.Get_HCAL_Energy())/track->p();
     if(tuple){tuple->BS_EIsol ->Fill(EoP,Event_Weight);}
      //     if(TypeMode_!=4){       if(EoP>GlobalMaxEIsol)return false;     }
     if(EoP>GlobalMaxEIsol)return false;
     if(tuple){tuple->EIsol   ->Fill(0.0,Event_Weight);}
     
     // relative tracker isolation
     if (tuple) {  tuple->BS_SumpTOverpT->Fill(hscpIso.Get_TK_SumEt()/track->pt(), Event_Weight); }
      //     if(TypeMode_==4) { if(hscpIso.Get_TK_SumEt()/track->pt()>GlobalMaxRelTIsol)return false;   }
     if(hscpIso.Get_TK_SumEt()/track->pt()>GlobalMaxRelTIsol)return false;
     if (tuple) {  tuple->SumpTOverpT   ->Fill(0.0,Event_Weight);} 
   }

   if(tuple){tuple->BS_Pterr ->Fill(track->ptError()/track->pt(),Event_Weight);}
   if(TypeMode_!=3 && (track->ptError()/track->pt())>GlobalMaxPterr)return false;
   //mk if(MassErr > 0 && MassErr > 2.2)return false; //FIXME jozze -- cut on relative mass error in units of 8*MassErr/Mass

   if(std::max(0.0,track->pt())<GlobalMinPt)return false;
   if(tuple){tuple->Pterr   ->Fill(0.0,Event_Weight);}

   //Find distance to nearest segment on opposite side of detector
   double minPhi=0.0, minEta=0.0;
   double segSep=SegSep(hscp, iEvent, minPhi, minEta);

   if(tuple){
     tuple->BS_SegSep->Fill(segSep, Event_Weight);
     tuple->BS_SegMinPhiSep->Fill(minPhi, Event_Weight);
     tuple->BS_SegMinEtaSep->Fill(minEta, Event_Weight);
     //Plotting segment separation depending on whether track passed dz cut
     if(fabs(dz)>GlobalMaxDZ) {
       tuple->BS_SegMinEtaSep_FailDz->Fill(minEta, Event_Weight);
     }
     else {
       tuple->BS_SegMinEtaSep_PassDz->Fill(minEta, Event_Weight);
     }
     //Plots for tracking failing Eta Sep cut
     if(fabs(minEta)<minSegEtaSep) {
       //Needed to compare dz distribution of cosmics in pure cosmic and main sample
       tuple->BS_Dz_FailSep->Fill(dz);
     }
   }



   //Now cut Eta separation
   //if(TypeMode_==3 && fabs(minEta)<minSegEtaSep) return false;
   //WAIT//if(tuple){tuple->SegSep->Fill(0.0,Event_Weight);}

   if(tuple) {
     //Plots for tracks in dz control region
     if(fabs(dz)>CosmicMinDz && fabs(dz)<CosmicMaxDz && !muon->isGlobalMuon()) {
       tuple->BS_Pt_FailDz->Fill(track->pt(), Event_Weight);
       //WAIT//tuple->BS_TOF_FailDz->Fill(tof->inverseBeta(), Event_Weight);
       if(fabs(track->eta())>CSCRegion) {
	 //WAIT//tuple->BS_TOF_FailDz_CSC->Fill(tof->inverseBeta(), Event_Weight);
	 tuple->BS_Pt_FailDz_CSC->Fill(track->pt(), Event_Weight);
       }
       else if(fabs(track->eta())<DTRegion) {
	 //WAIT//tuple->BS_TOF_FailDz_DT->Fill(tof->inverseBeta(), Event_Weight);
	 tuple->BS_Pt_FailDz_DT->Fill(track->pt(), Event_Weight);
       }
     }
     //Plots of dz
     tuple->BS_Dz->Fill(dz, Event_Weight);
     if(fabs(track->eta())>CSCRegion) tuple->BS_Dz_CSC->Fill(dz,Event_Weight);
     else if(fabs(track->eta())<DTRegion) tuple->BS_Dz_DT->Fill(dz,Event_Weight);
     tuple->BS_EtaDz->Fill(track->eta(),dz,Event_Weight);
   }


   //Split into different dz regions, each different region used to predict cosmic background and find systematic
   if(TypeMode_==3 && !muon->isGlobalMuon() && tuple) {
     //WAIT//int DzType=-1;
     //WAIT//if(fabs(dz)<GlobalMaxDZ) DzType=0;
     //WAIT//else if(fabs(dz)<30) DzType=1;
     //WAIT//else if(fabs(dz)<50) DzType=2;
     //WAIT//else if(fabs(dz)<70) DzType=3;
     //WAIT//if(fabs(dz)>CosmicMinDz && fabs(dz)<CosmicMaxDz) DzType=4;
     //WAIT//if(fabs(dz)>CosmicMaxDz) DzType=5;

     //Count number of tracks in dz sidebands passing the TOF cut
     //The pt cut is not applied to increase statistics
     for(unsigned int CutIndex=0;CutIndex<CutPt_.size();CutIndex++){
       //WAIT//if(tof->inverseBeta()>=CutTOF[CutIndex]) {
	 //WAIT//tuple->H_D_DzSidebands->Fill(CutIndex, DzType);
       //WAIT//}
     }
   }

   TreeDZ = dz;
   bool DZSB = false;
   if(TypeMode_!=5 && fabs(dz)>GlobalMaxDZ) return false;
   if(TypeMode_==5 && fabs(dz)>4) return false;
   if(TypeMode_==5 && fabs(dz)>GlobalMaxDZ) DZSB = true;
   if(tuple){tuple->Dz  ->Fill(0.0,Event_Weight);}

   if(TypeMode_==3 && fabs(minEta)<minSegEtaSep) return false;
   if(tuple)tuple->BS_Phi->Fill(track->phi(),Event_Weight);
   if(TypeMode_==3 && fabs(track->phi())>1.2 && fabs(track->phi())<1.9) return false;

    //skip HSCP that are compatible with cosmics.
    if(tuple)tuple->BS_OpenAngle->Fill(OpenAngle,Event_Weight);

    bool OASB = false;
    if(TypeMode_==5 && OpenAngle>=2.8)OASB = true;

   isCosmicSB = DXYSB && DZSB && OASB;
   isSemiCosmicSB = (!isCosmicSB && (DXYSB || DZSB || OASB));
 
   if(tuple){if(dedxSObj) tuple->BS_EtaIs->Fill(track->eta(),dedxSObj->dEdx(),Event_Weight);
          if(dedxMObj) tuple->BS_EtaIm->Fill(track->eta(),dedxMObj->dEdx(),Event_Weight);
          tuple->BS_EtaP ->Fill(track->eta(),track->p(),Event_Weight);
          tuple->BS_EtaPt->Fill(track->eta(),track->pt(),Event_Weight);
          //WAIT//tuple->BS_EtaTOF->Fill(track->eta(),tof->inverseBeta(),Event_Weight);
   }

   if(tuple){if(GenBeta>=0)tuple->Beta_PreselectedC->Fill(GenBeta, Event_Weight);
          if(DZSB  && OASB)tuple->BS_Dxy_Cosmic->Fill(dxy, Event_Weight);
          if(DXYSB && OASB)tuple->BS_Dz_Cosmic->Fill(dz, Event_Weight);
          if(DXYSB && DZSB)tuple->BS_OpenAngle_Cosmic->Fill(OpenAngle,Event_Weight);


          //WAIT// 
          TVector3 outerHit = getOuterHitPos(dedxHits);
          TVector3 vertex(vertexColl[highestPtGoodVertex].position().x(), vertexColl[highestPtGoodVertex].position().y(), vertexColl[highestPtGoodVertex].position().z());
          tuple->BS_LastHitDXY  ->Fill((outerHit).Perp(),Event_Weight);
          tuple->BS_LastHitD3D  ->Fill((outerHit).Mag(),Event_Weight);

          tuple->BS_P  ->Fill(track->p(),Event_Weight);
          tuple->BS_Pt ->Fill(track->pt(),Event_Weight);
          if(PUA)tuple->BS_Pt_PUA ->Fill(track->pt(),Event_Weight);
          if(PUB)tuple->BS_Pt_PUB ->Fill(track->pt(),Event_Weight);
          if(DXYSB && DZSB && OASB) tuple->BS_Pt_Cosmic->Fill(track->pt(),Event_Weight);

	  if(fabs(track->eta())<DTRegion) tuple->BS_Pt_DT->Fill(track->pt(),Event_Weight);
	  else tuple->BS_Pt_CSC->Fill(track->pt(),Event_Weight);

          double RecoQoPt = track->charge()/track->pt();
          if(!hscp.trackRef().isNull() && hscp.trackRef()->pt()>200) {
            double InnerRecoQoPt = hscp.trackRef()->charge()/hscp.trackRef()->pt();
            tuple->BS_InnerInvPtDiff->Fill((RecoQoPt-InnerRecoQoPt)/InnerRecoQoPt,Event_Weight);
          }

          if(dedxSObj) tuple->BS_Is ->Fill(dedxSObj->dEdx(),Event_Weight);
          if(dedxSObj && PUA) tuple->BS_Is_PUA ->Fill(dedxSObj->dEdx(),Event_Weight);
          if(dedxSObj && PUB) tuple->BS_Is_PUB ->Fill(dedxSObj->dEdx(),Event_Weight);
          if(dedxSObj && DXYSB && DZSB && OASB) tuple->BS_Is_Cosmic->Fill(dedxSObj->dEdx(),Event_Weight);
          if(dedxSObj) tuple->BS_Im ->Fill(dedxMObj->dEdx(),Event_Weight);
          if(dedxSObj && PUA) tuple->BS_Im_PUA ->Fill(dedxMObj->dEdx(),Event_Weight);
          if(dedxSObj && PUB) tuple->BS_Im_PUB ->Fill(dedxMObj->dEdx(),Event_Weight);

	    //WAIT//tuple->BS_TOF->Fill(tof->inverseBeta(),Event_Weight);
            //WAIT//if(PUA)tuple->BS_TOF_PUA->Fill(tof->inverseBeta(),Event_Weight);
            //WAIT//if(PUB)tuple->BS_TOF_PUB->Fill(tof->inverseBeta(),Event_Weight);
	    //WAIT//if(dttof->nDof()>6) tuple->BS_TOF_DT->Fill(dttof->inverseBeta(),Event_Weight);
            //WAIT//if(csctof->nDof()>6) tuple->BS_TOF_CSC->Fill(csctof->inverseBeta(),Event_Weight);
            //WAIT//tuple->BS_PtTOF->Fill(track->pt() ,tof->inverseBeta(),Event_Weight);

          if(dedxSObj) {
	    tuple->BS_PIs  ->Fill(track->p()  ,dedxSObj->dEdx(),Event_Weight);
            tuple->BS_PImHD->Fill(track->p()  ,dedxMObj->dEdx(),Event_Weight);
            tuple->BS_PIm  ->Fill(track->p()  ,dedxMObj->dEdx(),Event_Weight);
            tuple->BS_PtIs ->Fill(track->pt() ,dedxSObj->dEdx(),Event_Weight);
            tuple->BS_PtIm ->Fill(track->pt() ,dedxMObj->dEdx(),Event_Weight);
	  }
          //WAIT//if(dedxSObj)tuple->BS_TOFIs->Fill(tof->inverseBeta(),dedxSObj->dEdx(),Event_Weight);
          //WAIT//if(dedxSObj)tuple->BS_TOFIm->Fill(tof->inverseBeta(),dedxMObj->dEdx(),Event_Weight);

	  //Muon only prediction binned depending on where in the detector the track is and how many muon stations it has
	  //Binning not used for other analyses
	  int bin=-1;
	  if(TypeMode_==3) {
	    if(fabs(track->eta())<DTRegion) bin=muonStations(track->hitPattern())-2;
	    else bin=muonStations(track->hitPattern())+1;
	    tuple->BS_Pt_Binned[to_string(bin)] ->Fill(track->pt(),Event_Weight);
	    //WAIT//tuple->BS_TOF_Binned[to_string(bin)]->Fill(tof->inverseBeta(),Event_Weight);
	  }
   }
   if(tuple){tuple->Basic  ->Fill(0.0,Event_Weight);}

   return true;
}

//=============================================================
//
//     Selection
//
//=============================================================
bool Analyzer::passSelection(
         const susybsm::HSCParticle& hscp,  
         const reco::DeDxData* dedxSObj, 
         const reco::DeDxData* dedxMObj, 
         const reco::MuonTimeExtra* tof, 
         const edm::Event& iEvent,
         float Event_Weight,
         const int& CutIndex, 
         Tuple* &tuple, 
         const bool isFlip, 
         const double& GenBeta, 
         bool RescaleP, 
         const double& RescaleI, 
         const double& RescaleT)
{
   reco::TrackRef   track;
   if(TypeMode_!=3) track = hscp.trackRef();
   else {
     reco::MuonRef muon = hscp.muonRef();
     if(muon.isNull()) return false;
     track = muon->standAloneMuon();
   }
   if(track.isNull())return false;

   double MuonTOF = GlobalMinTOF;
   if(tof){
      MuonTOF = tof->inverseBeta();
   }

   double Is=0;   if(dedxSObj) Is=dedxSObj->dEdx();
   double Ih=0;   if(dedxMObj) Ih=dedxMObj->dEdx();
   double Ick=0; // if(dedxMObj) Ick=GetIck(Ih,isMC);

   double PtCut=CutPt_[CutIndex];
   double ICut=CutI_[CutIndex];
   double TOFCut=CutTOF_[CutIndex];
   if(isFlip) {
     PtCut=CutPt_Flip_[CutIndex];
     ICut=CutI_Flip_[CutIndex];
     TOFCut=CutTOF_Flip_[CutIndex];
   }

   if(RescaleP){
     if(RescaledPt(track->pt(),track->eta(),track->phi(),track->charge())<PtCut)return false;
     //if(std::max(0.0,RescaledPt(track->pt() - track->ptError(),track->eta(),track->phi(),track->charge()))<CutPt_[CutIndex])return false; 
   }else{
     if(track->pt()<PtCut)return false;
     //if(std::max(0.0,(track->pt() - track->ptError()))<CutPt_[CutIndex])return false;
   } 
   if(tuple){tuple->Pt    ->Fill(CutIndex,Event_Weight);
          if(GenBeta>=0)tuple->Beta_SelectedP->Fill(CutIndex,GenBeta, Event_Weight);
   }

   if(TypeMode_!=3 && Is+RescaleI<ICut)return false;

   if(tuple){tuple->I    ->Fill(CutIndex,Event_Weight);
          if(GenBeta>=0)tuple->Beta_SelectedI->Fill(CutIndex, GenBeta, Event_Weight);
   }

   if((TypeMode_>1  && TypeMode_!=5) && !isFlip && MuonTOF+RescaleT<TOFCut)return false;
   if((TypeMode_>1  && TypeMode_!=5) && isFlip && MuonTOF+RescaleT>TOFCut)return false;

   if(tuple){tuple->TOF  ->Fill(CutIndex,Event_Weight);
          if(GenBeta>=0)tuple->Beta_SelectedT->Fill(CutIndex, GenBeta, Event_Weight);
          tuple->AS_P  ->Fill(CutIndex,track->p(),Event_Weight);
          tuple->AS_Pt ->Fill(CutIndex,track->pt(),Event_Weight);
          tuple->AS_Is ->Fill(CutIndex,Is,Event_Weight);
          tuple->AS_Im ->Fill(CutIndex,Ih,Event_Weight);
          tuple->AS_TOF->Fill(CutIndex,MuonTOF,Event_Weight);
          //tuple->AS_EtaIs->Fill(CutIndex,track->eta(),Is,Event_Weight);
          //tuple->AS_EtaIm->Fill(CutIndex,track->eta(),Ih,Event_Weight);
          //tuple->AS_EtaP ->Fill(CutIndex,track->eta(),track->p(),Event_Weight);
          //tuple->AS_EtaPt->Fill(CutIndex,track->eta(),track->pt(),Event_Weight);
          tuple->AS_PIs  ->Fill(CutIndex,track->p()  ,Is,Event_Weight);
          tuple->AS_PIm  ->Fill(CutIndex,track->p()  ,Ih,Event_Weight);
          tuple->AS_PtIs ->Fill(CutIndex,track->pt() ,Is,Event_Weight);
          tuple->AS_PtIm ->Fill(CutIndex,track->pt() ,Ih,Event_Weight);
          tuple->AS_TOFIs->Fill(CutIndex,MuonTOF     ,Is,Event_Weight);
          tuple->AS_TOFIm->Fill(CutIndex,MuonTOF     ,Ih,Event_Weight);
   }
   return true;      
}

//=============================================================
//
//     Trigger-Selection
//
//=============================================================
bool Analyzer::passTrigger(const edm::Event& iEvent, bool isData, bool isCosmic, L1BugEmulator* emul){

   edm::Handle<edm::TriggerResults> triggerH;
   iEvent.getByToken(triggerResultsToken_,triggerH); 
   bool valid = triggerH.isValid(); 
   if (not valid){
      edm::LogError("Analyzer") << "HLT TriggerResults not found!";
      return false;
   }

   const edm::TriggerNames& triggerNames = iEvent.triggerNames(*triggerH);

   bool metTrig = PassTriggerPatterns(triggerH, triggerNames, trigger_met_);
   bool muTrig  = PassTriggerPatterns(triggerH, triggerNames, trigger_mu_);

   if (!metTrig && muTrig) TrigInfo_ = 1; // mu only
   if (metTrig && !muTrig) TrigInfo_ = 2; // met only
   if (metTrig && muTrig)  TrigInfo_ = 3; // mu and met*/

   if (metTrig) return true;
   if (muTrig){
      if (!isData && emul){
         edm::Handle < vector<reco::Muon> > muonCollH;
         iEvent.getByToken(muonToken_,muonCollH);
         if(!muonCollH.isValid()) return false;
         bool KeepEvent=false;
         for (unsigned int c=0;c<muonCollH->size();c++){
            reco::MuonRef muon = reco::MuonRef(muonCollH.product(), c);
            if (muon.isNull()) continue;
            if (muon->track().isNull()) continue; 
            if (emul->PassesL1Inefficiency(muon->track()->pt(), std::fabs(muon->track()->eta()))){
               KeepEvent=true;
               break;
            }
         }
         return KeepEvent;
      }
      else return true;
   }

   return false; //FIXME triggers bellow will need to be adapted based on Run2 trigger menu
   /*
   //for(unsigned int i=0;i<tr.size();i++){
   //printf("Path %3i %50s --> %1i\n",i, tr.triggerName(i).c_str(),tr.accept(i));
   //}fflush(stdout);

   //if(tr.accept("HSCPHLTTriggerMetDeDxFilter"))return true;
   //if(tr.accept("HSCPHLTTriggerMuDeDxFilter"))return true;
   if(tr.accept("HSCPHLTTriggerMuFilter"))return true;
   if(tr.accept("HSCPHLTTriggerPFMetFilter"))return true;

   //Could probably use this trigger for the other analyses as well
   if(TypeMode_==3){
      if(tr.size()== tr.triggerIndex("HSCPHLTTriggerL2MuFilter")) return false;
      if(tr.accept(tr.triggerIndex("HSCPHLTTriggerL2MuFilter")))  return true;

      //Only accepted if looking for cosmic events
      if(isCosmic) {
         if(tr.size()== tr.triggerIndex("HSCPHLTTriggerCosmicFilter")) return false;
         if(tr.accept(tr.triggerIndex("HSCPHLTTriggerCosmicFilter"))) return true;
      }
   }*/
   return false;
}