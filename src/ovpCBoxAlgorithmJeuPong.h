#ifndef __OpenViBEPlugins_BoxAlgorithm_JeuPong_H__
#define __OpenViBEPlugins_BoxAlgorithm_JeuPong_H__

//You may have to change this path to match your folder organisation
#include "ovp_defines.h"

#include <openvibe/ov_all.h>
#include <toolkit/ovtk_all.h>

#include <visualization-toolkit/ovviz_all.h>

#include <gtk/gtk.h>

#include <vector>
#include <string>
#include <map>
#include <deque>


// The unique identifiers for the box and its descriptor.
// Identifier are randomly chosen by the skeleton-generator.
#define OVP_ClassId_BoxAlgorithm_JeuPong (0xda3b879b, 0x3cb4dbd5)
#define OVP_ClassId_BoxAlgorithm_JeuPongDesc (0xbc296e8f, 0x89a87eea)
#define OV_AttributeId_Box_FlagIsUnstable OpenViBE::CIdentifier(0x666FFFFF, 0x666FFFFF)

namespace TCPTagging
{
	class IStimulusSender; // fwd declare
};
namespace OpenViBEPlugins
{
	namespace SignalProcessing
	{
		/**
		 * \class CBoxAlgorithmJeuPong
		 * \author Tristan (Sorbonne Université)
		 * \date Sun Jan 20 16:58:35 2019
		 * \brief The class CBoxAlgorithmJeuPong describes the box JeuPong.
		 *
		 */

		enum EArrowDirection
		{
			EArrowDirection_None = 0,
			EArrowDirection_Left,
			EArrowDirection_Right,
			EArrowDirection_Up,
			EArrowDirection_Down,
		};

		enum EGrazVisualizationState
		{
			EGrazVisualizationState_Idle,
			EGrazVisualizationState_Reference,
			EGrazVisualizationState_Cue,
			EGrazVisualizationState_ContinousFeedback
		};
		
	class CBoxAlgorithmJeuPong :
		virtual public OpenViBEToolkit::TBoxAlgorithm<OpenViBE::Plugins::IBoxAlgorithm>
	{

	public:

		CBoxAlgorithmJeuPong(void);

		virtual void release(void) { delete this; }

		virtual bool initialize();
		virtual bool uninitialize();
		virtual bool processInput(OpenViBE::uint32 ui32InputIndex);
		virtual bool process();

		virtual void redraw();
		virtual void resize(OpenViBE::uint32 ui32Width, OpenViBE::uint32 ui32Height);

	public:

		void flushQueue(void);					// Sends all accumulated stimuli to the TCP Tagging

	protected:

	

		virtual void setMatrixBuffer(const OpenViBE::float64* pBuffer);

		

		virtual void drawReferenceCross();
		virtual void drawArrow(EArrowDirection eDirection);
		virtual void drawBar();
		virtual void drawAccuracy();
		virtual void updateConfusionMatrix(OpenViBE::float64 f64Prediction);
		virtual OpenViBE::float64 aggregatePredictions(bool bIncludeAll);

		virtual void drawField();
		virtual void drawTarget(EArrowDirection eDirection);
		virtual void drawBall();


		_IsDerivedFromClass_Final_(OpenViBE::Plugins::IBoxAlgorithm, OVP_ClassId_BoxAlgorithm_JeuPong)

	public:
		//! The Builder handler used to create the interface
		::GtkBuilder* m_pBuilderInterface;

		GtkWidget * m_pMainWindow;

		GtkWidget * m_pDrawingArea;

		//ebml
		OpenViBEToolkit::TStimulationDecoder<CBoxAlgorithmJeuPong> m_oStimulationDecoder;
		OpenViBEToolkit::TStreamedMatrixDecoder<CBoxAlgorithmJeuPong> m_oMatrixDecoder;

		EGrazVisualizationState m_eCurrentState;
		EArrowDirection m_eCurrentDirection;

		OpenViBE::float64 m_f64MaxAmplitude;
		OpenViBE::float64 m_f64BarScale;

		//Start and end time of the last buffer
		OpenViBE::uint64 m_ui64StartTime;
		OpenViBE::uint64 m_ui64EndTime;

		bool m_bTwoValueInput;

		GdkPixbuf * m_pOriginalBar;
		GdkPixbuf * m_pLeftBar;
		GdkPixbuf * m_pRightBar;

		GdkPixbuf * m_pOriginalLeftArrow;
		GdkPixbuf * m_pOriginalRightArrow;
		GdkPixbuf * m_pOriginalUpArrow;
		GdkPixbuf * m_pOriginalDownArrow;

		GdkPixbuf * m_pLeftArrow;
		GdkPixbuf * m_pRightArrow;
		GdkPixbuf * m_pUpArrow;
		GdkPixbuf * m_pDownArrow;

		GdkColor m_oBackgroundColor;
		GdkColor m_oForegroundColor;

		std::deque<OpenViBE::float64> m_vAmplitude; // predictions for the current trial

		bool m_bShowInstruction;
		bool m_bShowFeedback;
		bool m_bDelayFeedback;
		bool m_bShowAccuracy;
		bool m_bPositiveFeedbackOnly;

		OpenViBE::uint64 m_i64PredictionsToIntegrate;

		OpenViBE::CMatrix m_oConfusion;

		// For queuing stimulations to the TCP Tagging
		std::vector< OpenViBE::uint64 > m_vStimuliQueue;
		guint m_uiIdleFuncTag;
		TCPTagging::IStimulusSender* m_pStimulusSender;

		OpenViBE::uint64 m_ui64LastStimulation;

	private:
		OpenViBEVisualizationToolkit::IVisualizationContext* m_visualizationContext = nullptr;
	};

		/**
		 * \class CBoxAlgorithmJeuPongDesc
		 * \author Tristan (Sorbonne Université)
		 * \date Sun Jan 20 16:58:35 2019
		 * \brief Descriptor of the box JeuPong.
		 *
		 */
		class CBoxAlgorithmJeuPongDesc : virtual public OpenViBE::Plugins::IBoxAlgorithmDesc
		{
		public:

			virtual void release(void) { }

			virtual OpenViBE::CString getName(void) const                { return OpenViBE::CString("JeuPong"); }
			virtual OpenViBE::CString getAuthorName(void) const          { return OpenViBE::CString("Tristan"); }
			virtual OpenViBE::CString getAuthorCompanyName(void) const   { return OpenViBE::CString("Sorbonne Université"); }
			virtual OpenViBE::CString getShortDescription(void) const    { return OpenViBE::CString("Ball controlled by thoughts"); }
			virtual OpenViBE::CString getDetailedDescription(void) const { return OpenViBE::CString("if you think left, the ball goes left, if you think right, the ball goes right"); }
			virtual OpenViBE::CString getCategory(void) const            { return OpenViBE::CString("Signal processing"); }
			virtual OpenViBE::CString getVersion(void) const             { return OpenViBE::CString("1.0"); }
			virtual OpenViBE::CString getStockItemName(void) const       { return OpenViBE::CString("gtk-fullscreen"); }

			virtual OpenViBE::CIdentifier getCreatedClass(void) const    { return OVP_ClassId_BoxAlgorithm_JeuPong; }
			virtual OpenViBE::Plugins::IPluginObject* create(void)       { return new OpenViBEPlugins::SignalProcessing::CBoxAlgorithmJeuPong; }
			
			/*
			virtual OpenViBE::Plugins::IBoxListener* createBoxListener(void) const               { return new CBoxAlgorithmJeuPongListener; }
			virtual void releaseBoxListener(OpenViBE::Plugins::IBoxListener* pBoxListener) const { delete pBoxListener; }
			*/
			virtual bool getBoxPrototype(
				OpenViBE::Kernel::IBoxProto& rBoxAlgorithmPrototype) const
			{
				rBoxAlgorithmPrototype.addInput("Stimulations",OV_TypeId_Stimulations);
				rBoxAlgorithmPrototype.addInput("Ampltude",OV_TypeId_StreamedMatrix);

				rBoxAlgorithmPrototype.addFlag(OpenViBE::Kernel::BoxFlag_CanModifyInput);
				rBoxAlgorithmPrototype.addFlag(OpenViBE::Kernel::BoxFlag_CanAddInput);
				
//No output specified.To add outputs use :
//rBoxAlgorithmPrototype.addOutput("OutputName",OV_TypeId_XXXX);

				//rBoxAlgorithmPrototype.addFlag(OpenViBE::Kernel::BoxFlag_CanModifyOutput);
				//rBoxAlgorithmPrototype.addFlag(OpenViBE::Kernel::BoxFlag_CanAddOutput);

				rBoxAlgorithmPrototype.addSetting("Show instruction",OV_TypeId_Boolean,"true");
				rBoxAlgorithmPrototype.addSetting("Show feedback",OV_TypeId_Boolean,"false");
				rBoxAlgorithmPrototype.addSetting("Delay feedback",OV_TypeId_Boolean,"false");
				rBoxAlgorithmPrototype.addSetting("Show accuracy",OV_TypeId_Boolean,"false");
				rBoxAlgorithmPrototype.addSetting("Predictions to integrate",OV_TypeId_Integer,"5");
				rBoxAlgorithmPrototype.addSetting("Positive feedback only",OV_TypeId_Boolean,"false");

				rBoxAlgorithmPrototype.addFlag(OpenViBE::Kernel::BoxFlag_CanModifySetting);
				rBoxAlgorithmPrototype.addFlag(OpenViBE::Kernel::BoxFlag_CanAddSetting);
				
				rBoxAlgorithmPrototype.addFlag(OV_AttributeId_Box_FlagIsUnstable);
				
				return true;
			}
			_IsDerivedFromClass_Final_(OpenViBE::Plugins::IBoxAlgorithmDesc, OVP_ClassId_BoxAlgorithm_JeuPongDesc);
		};
	};
};

#endif // __OpenViBEPlugins_BoxAlgorithm_JeuPong_H__
