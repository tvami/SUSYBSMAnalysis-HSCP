import FWCore.ParameterSet.Config as cms

process = cms.Process("HSCPAnalysis")

process.load("FWCore.MessageService.MessageLogger_cfi")
process.load('Configuration.StandardSequences.GeometryRecoDB_cff')
process.load('Configuration.StandardSequences.MagneticField_cff')
process.load('Configuration.StandardSequences.Services_cff')
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')
process.load('Configuration.StandardSequences.EndOfProcess_cff')


## MessageLogger
process.load("FWCore.MessageLogger.MessageLogger_cfi")
# If you run over many samples and you save the log, remember to reduce
# the size of the output by prescaling the report of the event number
#process.MessageLogger.cerr.FwkReport.reportEvery = 5000
#process.MessageLogger.cerr.default.limit = 10

process.options = cms.untracked.PSet(
#    wantSummary = cms.untracked.bool(True),
    SkipEvent = cms.untracked.vstring('MismatchedInputFiles'),
)

from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag.globaltag = "106X_dataRun2_v27"

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(10)
)

# /SingleMuon/Run2018*-12Nov2019_UL2018*/AOD
# /SingleMuon/Run2018D-12Nov2019_UL2018-v4/AOD
#root://cms-xrd-global.cern.ch//store/data/Run2018D/SingleMuon/AOD/12Nov2019_UL2018-v4/100000/63BF07E8-4AAA-DC4D-99DE-48B52FCB0C06.root
process.source = cms.Source("PoolSource",
    # replace 'myfile.root' with the source file you want to use
    fileNames = cms.untracked.vstring(
#         'file:63BF07E8-4AAA-DC4D-99DE-48B52FCB0C06.root'
         'root://cms-xrd-global.cern.ch//store/data/Run2018D/SingleMuon/AOD/12Nov2019_UL2018-v4/100000/63BF07E8-4AAA-DC4D-99DE-48B52FCB0C06.root'
    ),
    secondaryFileNames = cms.untracked.vstring(
         'root://cms-xrd-global.cern.ch//store/data/Run2018D/SingleMuon/ALCARECO/SiPixelCalSingleMuon-ForPixelALCARECO_UL2018-v1/250000/EE812551-E563-5B44-B6AC-F8190A191ABF.root',
    )
)

#---------------------- Refitter -----------------------
process.load("RecoVertex.BeamSpotProducer.BeamSpot_cff")
process.load("RecoTracker.TrackProducer.TrackRefitters_cff")
process.load("TrackingTools.TransientTrack.TransientTrackBuilder_cfi")
process.load("RecoTracker.MeasurementDet.MeasurementTrackerEventProducer_cfi")

process.MeasurementTrackerEvent.pixelClusterProducer = 'ALCARECOSiPixelCalSingleMuon'
process.MeasurementTrackerEvent.stripClusterProducer = 'ALCARECOSiPixelCalSingleMuon'
process.MeasurementTrackerEvent.inactivePixelDetectorLabels = cms.VInputTag()
process.MeasurementTrackerEvent.inactiveStripDetectorLabels = cms.VInputTag()

#process.TrackRefitter.src = cms.InputTag('globalMuons::RECO')
process.TrackRefitter.src = 'ALCARECOSiPixelCalSingleMuon'
#process.TrackRefitter.src = 'generalTracks'
process.TrackRefitter.TrajectoryInEvent = True
process.TrackRefitter.NavigationSchool = ''
process.TrackRefitter.TTRHBuilder = "WithAngleAndTemplate"

process.stage = cms.EDAnalyzer('ntuple'
     , format_file       = cms.string('AOD')
     , isdata            = cms.bool(True)
     , year              = cms.untracked.int32(2018)
     , primaryVertexColl  = cms.InputTag('offlinePrimaryVertices')  #->AOD
     , isotracks             = cms.InputTag("isolatedTracks")
     , tracks             = cms.InputTag("generalTracks")
     , collectionHSCP     = cms.InputTag("HSCParticleProducer")
     , isoHSCP0            = cms.InputTag("HSCPIsolation","R005")
     , isoHSCP1            = cms.InputTag("HSCPIsolation","R01")
     , isoHSCP2            = cms.InputTag("HSCPIsolation","R03")
     , isoHSCP3            = cms.InputTag("HSCPIsolation","R05")
     , dedx               = cms.InputTag("dedxHitInfo")
     , MiniDedx           = cms.InputTag("isolatedTracks")
     , dEdxHitInfoPrescale = cms.InputTag("dedxHitInfo","prescale")
     , muons              = cms.InputTag("muons")
     , muonTOF            = cms.InputTag("muons","combined")
     , muonTDT            = cms.InputTag("muons","dt")
     , muonTCSC           = cms.InputTag("muons","csc")
     , printOut           = cms.untracked.int32(-1)
     , GenPart            = cms.InputTag("genParticles")
     , runOnGS            = cms.bool(False)
     , stripSimLinks      = cms.InputTag("simSiStripDigis")
     , ROUList            = cms.vstring(
                                      'TrackerHitsTIBLowTof',  # hard-scatter (prompt) collection, module label g4simHits
                                      'TrackerHitsTIBHighTof',
                                      'TrackerHitsTIDLowTof',
                                      'TrackerHitsTIDHighTof',
                                      'TrackerHitsTOBLowTof',
                                      'TrackerHitsTOBHighTof',
                                      'TrackerHitsTECLowTof',
                                      'TrackerHitsTECHighTof'
                                      )
    , lumiScalerTag     = cms.InputTag("scalersRawToDigi")
    , doRecomputeMuTim  = cms.bool(False)  # no recomputation for MC !!
    , cscSegments       = cms.InputTag("cscSegments")
    , dt4DSegments      = cms.InputTag("dt4DSegments")
    , pfMet             = cms.InputTag("pfMet", "", "RECO")
    , pfCand            = cms.InputTag("particleFlow", "", "RECO")
    , pfJet             = cms.InputTag("ak4PFJetsCHS", "", "RECO")
    , L1muon            = cms.InputTag("gmtStage2Digis","Muon","RECO")
    , trajInputLabel    = cms.untracked.InputTag('TrackRefitter::HSCPAnalysis')
)


process.TFileService = cms.Service("TFileService",
     fileName = cms.string('nt_data_aod.root')
 )

process.load("SUSYBSMAnalysis.HSCP.HSCParticleProducer_cff")

# CMS Path
process.TrackRefitter_step = cms.Path(
  process.offlineBeamSpot*
  process.MeasurementTrackerEvent*
  process.TrackRefitter
)

process.HSCParticleProducer_step = cms.Path(process.HSCParticleProducerSeq)
process.Ntuple_step = cms.Path(process.stage)
process.endjob_step = cms.EndPath(process.endOfProcess)

process.schedule = cms.Schedule(
    process.TrackRefitter_step,
    process.HSCParticleProducer_step,
    process.Ntuple_step,
    process.endjob_step
)

#do not add changes to your config after this point (unless you know what you are doing)
from FWCore.ParameterSet.Utilities import convertToUnscheduled
process=convertToUnscheduled(process)

# Add early deletion of temporary data products to reduce peak memory need
from Configuration.StandardSequences.earlyDeleteSettings_cff import customiseEarlyDelete
process = customiseEarlyDelete(process)


