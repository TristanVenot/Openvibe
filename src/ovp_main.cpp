
#include <vector>
#include <openvibe/ov_all.h>
#include "ovp_defines.h"

#include "box-algorithms/ovpCBoxAlgorithmCSPSpatialFilterTrainer.h"		// ghent univ
#include "algorithms/ovpCAlgorithmUnivariateStatistics.h"				// gipsa
#include "box-algorithms/ovpCBoxAlgorithmUnivariateStatistics.h"		// gipsa
#include "box-algorithms/ovpCBoxAlgorithmSynchro.h"						// gipsa


// @BEGIN inserm-gpl
#include "algorithms/ovpCDetectingMinMax.h"
#include "box-algorithms/ovpCDetectingMinMaxBoxAlgorithm.h"

#include "box-algorithms/ovpCWindowingFunctions.h"
#include "box-algorithms/ovpCFastICA.h"
#include "box-algorithms/ovpCSpectralAnalysis.h"

#include "algorithms/ovpCApplyTemporalFilter.h"
#include "algorithms/ovpCComputeTemporalFilterCoefficients.h"
#include "box-algorithms/ovpCTemporalFilterBoxAlgorithm.h"
#include "box-algorithms/ovpCModTemporalFilterBoxAlgorithm.h"

#include "algorithms/ovpCDownsampling.h"
#include "box-algorithms/ovpCDownsamplingBoxAlgorithm.h"

#include "algorithms/ovpCDetectingMinMax.h"
#include "box-algorithms/ovpCDetectingMinMaxBoxAlgorithm.h"

#include "box-algorithms/ovpCBoxAlgorithmVisualInterface.h"

#include "ovpCBoxAlgorithmJeuPong.h"

// @END inserm-gpl

OVP_Declare_Begin();

#if defined TARGET_HAS_ThirdPartyITPP
	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CBoxAlgorithmCSPSpatialFilterTrainerDesc); // ghent univ
#endif		

	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CBoxAlgorithmSynchroDesc)		// gipsa
	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CAlgoUnivariateStatisticDesc);	// gipsa
	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CBoxUnivariateStatisticDesc);	// gipsa

		
// @BEGIN inserm-gpl
	rPluginModuleContext.getTypeManager().registerBitMaskType (OVP_TypeId_SpectralComponent, "Spectral component");
	rPluginModuleContext.getTypeManager().registerBitMaskEntry(OVP_TypeId_SpectralComponent, "Amplitude",      OVP_TypeId_SpectralComponent_Amplitude.toUInteger());
	rPluginModuleContext.getTypeManager().registerBitMaskEntry(OVP_TypeId_SpectralComponent, "Phase",          OVP_TypeId_SpectralComponent_Phase.toUInteger());
	rPluginModuleContext.getTypeManager().registerBitMaskEntry(OVP_TypeId_SpectralComponent, "Real part",      OVP_TypeId_SpectralComponent_RealPart.toUInteger());
	rPluginModuleContext.getTypeManager().registerBitMaskEntry(OVP_TypeId_SpectralComponent, "Imaginary part", OVP_TypeId_SpectralComponent_ImaginaryPart.toUInteger());

	rPluginModuleContext.getTypeManager().registerEnumerationType (OVP_TypeId_FilterMethod, "Filter method");
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FilterMethod, "Butterworth", OVP_TypeId_FilterMethod_Butterworth.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FilterMethod, "Chebychev",   OVP_TypeId_FilterMethod_Chebychev.toUInteger());

	rPluginModuleContext.getTypeManager().registerEnumerationType (OVP_TypeId_FilterType, "Filter type");
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FilterType, "Low Pass",  OVP_TypeId_FilterType_LowPass.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FilterType, "Band Pass", OVP_TypeId_FilterType_BandPass.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FilterType, "High Pass", OVP_TypeId_FilterType_HighPass.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FilterType, "Band Stop", OVP_TypeId_FilterType_BandStop.toUInteger());

	rPluginModuleContext.getTypeManager().registerEnumerationType (OVP_TypeId_WindowMethod, "Window method");
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_WindowMethod, "Hamming",     OVP_TypeId_WindowMethod_Hamming.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_WindowMethod, "Hanning",     OVP_TypeId_WindowMethod_Hanning.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_WindowMethod, "Hann",        OVP_TypeId_WindowMethod_Hann.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_WindowMethod, "Blackman",    OVP_TypeId_WindowMethod_Blackman.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_WindowMethod, "Triangular",  OVP_TypeId_WindowMethod_Triangular.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_WindowMethod, "Square root", OVP_TypeId_WindowMethod_SquareRoot.toUInteger());

	rPluginModuleContext.getTypeManager().registerEnumerationType (OVP_TypeId_FrequencyCutOffRatio, "Frequency cut off ratio");
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FrequencyCutOffRatio, "1/4", OVP_TypeId_FrequencyCutOffRatio_14.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FrequencyCutOffRatio, "1/3", OVP_TypeId_FrequencyCutOffRatio_13.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FrequencyCutOffRatio, "1/2", OVP_TypeId_FrequencyCutOffRatio_12.toUInteger());

	rPluginModuleContext.getTypeManager().registerEnumerationType (OVP_TypeId_MinMax, "Min/Max");
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_MinMax, "Min", OVP_TypeId_MinMax_Min.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_MinMax, "Max", OVP_TypeId_MinMax_Max.toUInteger());

	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OV_TypeId_BoxAlgorithmFlag, OV_AttributeId_Box_FlagIsUnstable.toString(), OV_AttributeId_Box_FlagIsUnstable.toUInteger());
	
#if defined TARGET_HAS_ThirdPartyITPP

	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CSpectralAnalysisDesc);

	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CFastICADesc);
	rPluginModuleContext.getTypeManager().registerEnumerationType (OVP_TypeId_FastICA_OperatingMode, "Operating mode");
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FastICA_OperatingMode, "PCA",    OVP_TypeId_FastICA_OperatingMode_PCA.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FastICA_OperatingMode, "Whiten", OVP_TypeId_FastICA_OperatingMode_Whiten.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FastICA_OperatingMode, "ICA",    OVP_TypeId_FastICA_OperatingMode_ICA.toUInteger());

	rPluginModuleContext.getTypeManager().registerEnumerationType (OVP_TypeId_FastICA_DecompositionType,     "Decomposition type");
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FastICA_DecompositionType,     "Symmetric",  OVP_TypeId_FastICA_DecompositionType_Symmetric.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FastICA_DecompositionType,     "Deflate",    OVP_TypeId_FastICA_DecompositionType_Deflate.toUInteger());

	rPluginModuleContext.getTypeManager().registerEnumerationType (OVP_TypeId_FastICA_Nonlinearity,     "Nonlinearity");
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FastICA_Nonlinearity,     "Pow3",     OVP_TypeId_FastICA_Nonlinearity_POW3.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FastICA_Nonlinearity,     "Tanh",     OVP_TypeId_FastICA_Nonlinearity_TANH.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FastICA_Nonlinearity,     "Gauss",    OVP_TypeId_FastICA_Nonlinearity_GAUSS.toUInteger());
	rPluginModuleContext.getTypeManager().registerEnumerationEntry(OVP_TypeId_FastICA_Nonlinearity,     "Skew",     OVP_TypeId_FastICA_Nonlinearity_SKEW.toUInteger());

	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CWindowingFunctionsDesc);
	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CComputeTemporalFilterCoefficientsDesc);
	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CTemporalFilterBoxAlgorithmDesc);
	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CModTemporalFilterBoxAlgorithmDesc);
	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CApplyTemporalFilterDesc);
#endif // TARGET_HAS_ThirdPartyITPP

	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CDownsamplingDesc);
	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CDownsamplingBoxAlgorithmDesc);
	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CDetectingMinMaxDesc);
	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CDetectingMinMaxBoxAlgorithmDesc);
// @END inserm-gpl
	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CBoxAlgorithmJeuPongDesc);

	OVP_Declare_New(OpenViBEPlugins::SignalProcessing::CBoxAlgorithmVisualInterfaceDesc);

OVP_Declare_End();
