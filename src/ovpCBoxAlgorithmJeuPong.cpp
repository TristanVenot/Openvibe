#include "ovpCBoxAlgorithmJeuPong.h"

#include <cmath>
#include <iostream>
#include <algorithm> // std::min, max

using namespace OpenViBE;
using namespace OpenViBE::Kernel;
using namespace OpenViBE::Plugins;

using namespace OpenViBEPlugins;
using namespace OpenViBEPlugins::SignalProcessing;


#include <iostream>
#include <fstream>
#include <cstdlib>

#include <sys/timeb.h>

#include <tcptagging/IStimulusSender.h>

namespace OpenViBEPlugins
{

	namespace SignalProcessing
	{
		// This callback flushes all accumulated stimulations to the TCP Tagging 
		// after the rendering has completed.
		gboolean flush_callback(gpointer pUserData)
		{
			reinterpret_cast<CBoxAlgorithmJeuPong*>(pUserData)->flushQueue();

			return false;	// Only run once
		}

		gboolean GrazVisualization_SizeAllocateCallback(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
		{
			reinterpret_cast<CBoxAlgorithmJeuPong*>(data)->resize((uint32)allocation->width, (uint32)allocation->height);
			return FALSE;
		}

		gboolean GrazVisualization_RedrawCallback(GtkWidget *widget, GdkEventExpose *event, gpointer data)
		{
			reinterpret_cast<CBoxAlgorithmJeuPong*>(data)->redraw();
			return TRUE;
		}

		// n.b. This reacts immediately to the received stimulation and doesn't use the date. Usually stimulations come from the upstream with
		// chunks having a very narrow time range, so its alright for Graz that changes state only rarely. Note if multiple stimulations are 
		// received in the same chunk, they'll be passed to TCP Tagging with the true delay between them lost.
		

		

		/**
		* Constructor
		*/
		CBoxAlgorithmJeuPong::CBoxAlgorithmJeuPong(void) :
			m_pBuilderInterface(NULL),
			m_pMainWindow(NULL),
			m_pDrawingArea(NULL),
			m_eCurrentState(EGrazVisualizationState_Idle),
			m_eCurrentDirection(EArrowDirection_None),
			m_f64MaxAmplitude(-DBL_MAX),
			m_f64BarScale(0.0),
			m_bTwoValueInput(false),
			m_pOriginalBar(NULL),
			m_pLeftBar(NULL),
			m_pRightBar(NULL),
			m_pOriginalLeftArrow(NULL),
			m_pOriginalRightArrow(NULL),
			m_pOriginalUpArrow(NULL),
			m_pOriginalDownArrow(NULL),
			m_pLeftArrow(NULL),
			m_pRightArrow(NULL),
			m_pUpArrow(NULL),
			m_pDownArrow(NULL),
			m_bShowInstruction(true),
			m_bShowFeedback(false),
			m_bDelayFeedback(false),
			m_bShowAccuracy(false),
			m_bPositiveFeedbackOnly(false),
			m_i64PredictionsToIntegrate(5),
			m_pStimulusSender(NULL),
			m_ui64LastStimulation(0)
		{
			m_oBackgroundColor.pixel = 0;
			m_oBackgroundColor.red = 0;//0xFFFF;
			m_oBackgroundColor.green = 0;//0xFFFF;
			m_oBackgroundColor.blue = 0;//0xFFFF;

			m_oForegroundColor.pixel = 0;
			m_oForegroundColor.red = 0;
			m_oForegroundColor.green = 0x8000;
			m_oForegroundColor.blue = 0;

			m_oConfusion.setDimensionCount(2);
			m_oConfusion.setDimensionSize(0, 2);
			m_oConfusion.setDimensionSize(1, 2);
		}

		boolean CBoxAlgorithmJeuPong::initialize()
		{
			m_bShowInstruction = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 0);
			m_bShowFeedback = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 1);
			m_bDelayFeedback = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 2);
			m_bShowAccuracy = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 3);
			m_i64PredictionsToIntegrate = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 4);
			m_bPositiveFeedbackOnly = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 5);

			m_pStimulusSender = nullptr;

			m_uiIdleFuncTag = 0;
			m_vStimuliQueue.clear();

			if (m_i64PredictionsToIntegrate<1)
			{
				this->getLogManager() << LogLevel_Error << "Number of predictions to integrate must be at least 1!";
				return false;
			}

			m_oStimulationDecoder.initialize(*this, 0);
			m_oMatrixDecoder.initialize(*this, 1);

			OpenViBEToolkit::Tools::Matrix::clearContent(m_oConfusion);

			//load the gtk builder interface
			m_pBuilderInterface = gtk_builder_new(); // glade_xml_new(OpenViBE::Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization.ui", NULL, NULL);
			gtk_builder_add_from_file(m_pBuilderInterface, OpenViBE::Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization.ui", NULL);

			if (!m_pBuilderInterface)
			{
				this->getLogManager() << LogLevel_Error << "Error: couldn't load the interface!";
				return false;
			}

			gtk_builder_connect_signals(m_pBuilderInterface, NULL);

			m_pDrawingArea = GTK_WIDGET(gtk_builder_get_object(m_pBuilderInterface, "GrazVisualizationDrawingArea"));
			g_signal_connect(G_OBJECT(m_pDrawingArea), "expose_event", G_CALLBACK(GrazVisualization_RedrawCallback), this);
			g_signal_connect(G_OBJECT(m_pDrawingArea), "size-allocate", G_CALLBACK(GrazVisualization_SizeAllocateCallback), this);

#if 0
			//does nothing on the main window if the user tries to close it
			g_signal_connect(G_OBJECT(gtk_builder_get_object(m_pBuilderInterface, "GrazVisualizationWindow")),
				"delete_event",
				G_CALLBACK(gtk_widget_do_nothing), NULL);

			//creates the window
			m_pMainWindow = GTK_WIDGET(gtk_builder_get_object(m_pBuilderInterface, "GrazVisualizationWindow"));
#endif

			//set widget bg color
			gtk_widget_modify_bg(m_pDrawingArea, GTK_STATE_NORMAL, &m_oBackgroundColor);
			gtk_widget_modify_bg(m_pDrawingArea, GTK_STATE_PRELIGHT, &m_oBackgroundColor);
			gtk_widget_modify_bg(m_pDrawingArea, GTK_STATE_ACTIVE, &m_oBackgroundColor);

			gtk_widget_modify_fg(m_pDrawingArea, GTK_STATE_NORMAL, &m_oForegroundColor);
			gtk_widget_modify_fg(m_pDrawingArea, GTK_STATE_PRELIGHT, &m_oForegroundColor);
			gtk_widget_modify_fg(m_pDrawingArea, GTK_STATE_ACTIVE, &m_oForegroundColor);

			//arrows
			m_pOriginalLeftArrow = gdk_pixbuf_new_from_file_at_size(OpenViBE::Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization-leftArrow.png", -1, -1, NULL);
			m_pOriginalRightArrow = gdk_pixbuf_new_from_file_at_size(OpenViBE::Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization-rightArrow.png", -1, -1, NULL);
			m_pOriginalUpArrow = gdk_pixbuf_new_from_file_at_size(OpenViBE::Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization-upArrow.png", -1, -1, NULL);
			m_pOriginalDownArrow = gdk_pixbuf_new_from_file_at_size(OpenViBE::Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization-downArrow.png", -1, -1, NULL);

			if (!m_pOriginalLeftArrow || !m_pOriginalRightArrow || !m_pOriginalUpArrow || !m_pOriginalDownArrow)
			{
				this->getLogManager() << LogLevel_Error << "Error couldn't load arrow resource files!\n";

				return false;
			}

			//bar
			m_pOriginalBar = gdk_pixbuf_new_from_file_at_size(OpenViBE::Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization-bar.png", -1, -1, NULL);
			if (!m_pOriginalBar)
			{
				this->getLogManager() << LogLevel_Error << "Error couldn't load bar resource file!\n";

				return false;
			}

#if 0
			gtk_widget_show_all(m_pMainWindow);
#endif
			m_visualizationContext = dynamic_cast<OpenViBEVisualizationToolkit::IVisualizationContext*>(this->createPluginObject(OVP_ClassId_Plugin_VisualizationContext));
			m_visualizationContext->setWidget(*this, m_pDrawingArea);

			m_pStimulusSender = TCPTagging::createStimulusSender();

			if (!m_pStimulusSender->connect("localhost", "15361"))
			{
				this->getLogManager() << LogLevel_Warning << "Unable to connect to AS's TCP Tagging plugin, stimuli wont be forwarded.\n";
			}

			return true;
		}

		boolean CBoxAlgorithmJeuPong::uninitialize()
		{
			if (m_uiIdleFuncTag)
			{
				m_vStimuliQueue.clear();
				g_source_remove(m_uiIdleFuncTag);
				m_uiIdleFuncTag = 0;
			}

			if (m_pStimulusSender)
			{
				delete m_pStimulusSender;
			}

			m_oStimulationDecoder.uninitialize();
			m_oMatrixDecoder.uninitialize();

#if 0
			//destroy the window and its children
			if (m_pMainWindow)
			{
				gtk_widget_destroy(m_pMainWindow);
				m_pMainWindow = nullptr;
			}
#endif
			//destroy drawing area
			if (m_pDrawingArea)
			{
				gtk_widget_destroy(m_pDrawingArea);
				m_pDrawingArea = nullptr;
			}

			/* unref the xml file as it's not needed anymore */
			g_object_unref(G_OBJECT(m_pBuilderInterface));
			m_pBuilderInterface = nullptr;

			if (m_pOriginalBar){ g_object_unref(G_OBJECT(m_pOriginalBar)); }
			if (m_pLeftBar){ g_object_unref(G_OBJECT(m_pLeftBar)); }
			if (m_pRightBar){ g_object_unref(G_OBJECT(m_pRightBar)); }
			if (m_pLeftArrow){ g_object_unref(G_OBJECT(m_pLeftArrow)); }
			if (m_pRightArrow){ g_object_unref(G_OBJECT(m_pRightArrow)); }
			if (m_pUpArrow){ g_object_unref(G_OBJECT(m_pUpArrow)); }
			if (m_pDownArrow){ g_object_unref(G_OBJECT(m_pDownArrow)); }
			if (m_pOriginalLeftArrow){ g_object_unref(G_OBJECT(m_pOriginalLeftArrow)); }
			if (m_pOriginalRightArrow){ g_object_unref(G_OBJECT(m_pOriginalRightArrow)); }
			if (m_pOriginalUpArrow){ g_object_unref(G_OBJECT(m_pOriginalUpArrow)); }
			if (m_pOriginalDownArrow){ g_object_unref(G_OBJECT(m_pOriginalDownArrow)); }

			if (m_visualizationContext)
			{
				this->releasePluginObject(m_visualizationContext);
				m_visualizationContext = nullptr;
			}

			return true;
		}

		boolean CBoxAlgorithmJeuPong::processInput(uint32 ui32InputIndex)
		{
			getBoxAlgorithmContext()->markAlgorithmAsReadyToProcess();
			return true;
		}

		boolean CBoxAlgorithmJeuPong::process()                                                              // Processs complet
		{
			const IBoxIO* l_pBoxIO = getBoxAlgorithmContext()->getDynamicBoxContext();

			for (uint32 chunk = 0; chunk<l_pBoxIO->getInputChunkCount(0); chunk++)
			{
				m_oStimulationDecoder.decode(chunk);
				if (m_oStimulationDecoder.isBufferReceived())
				{
					const IStimulationSet* l_pStimulationSet = m_oStimulationDecoder.getOutputStimulationSet();
					for (uint32 s = 0; s<l_pStimulationSet->getStimulationCount(); s++)
					{
						
					}
				}
			}

			for (uint32 chunk = 0; chunk<l_pBoxIO->getInputChunkCount(1); chunk++)
			{
				m_oMatrixDecoder.decode(chunk);
				if (m_oMatrixDecoder.isHeaderReceived())
				{
					const IMatrix* l_pMatrix = m_oMatrixDecoder.getOutputMatrix();

					if (l_pMatrix->getDimensionCount() == 0)
					{
						this->getLogManager() << LogLevel_Error << "Error, dimension count is 0 for Amplitude input !\n";
						return false;
					}

					if (l_pMatrix->getDimensionCount() > 1)
					{
						for (uint32 k = 1; k<l_pMatrix->getDimensionSize(k); k++)
						{
							if (l_pMatrix->getDimensionSize(k) > 1)
							{
								this->getLogManager() << LogLevel_Error << "Error, only column vectors supported as Amplitude!\n";
								return false;
							}
						}
					}

					if (l_pMatrix->getDimensionSize(0) == 0)
					{
						this->getLogManager() << LogLevel_Error << "Error, need at least 1 dimension in Amplitude input !\n";
						return false;
					}
					else if (l_pMatrix->getDimensionSize(0) >= 2)
					{
						this->getLogManager() << LogLevel_Trace << "Got 2 or more dimensions for feedback, feedback will be the difference between the first two.\n";
						m_bTwoValueInput = true;
					}
				}

				if (m_oMatrixDecoder.isBufferReceived())
				{
					setMatrixBuffer(m_oMatrixDecoder.getOutputMatrix()->getBuffer());
				}

			}

			// After any possible rendering, we flush the accumulated stimuli. The default idle func is low priority, so it should be run after rendering by gtk.
			// Only register a single idle func, if the previous is there its just as good
			if (m_uiIdleFuncTag == 0)
			{
				m_uiIdleFuncTag = g_idle_add(flush_callback, this);
			}

			return true;
		}

		void CBoxAlgorithmJeuPong::redraw()
		{
			switch (m_eCurrentState)
			{
			case EGrazVisualizationState_Reference:
				drawReferenceCross();
				break;

			case EGrazVisualizationState_Cue:
				drawReferenceCross();
				drawArrow(m_bShowInstruction ? m_eCurrentDirection : EArrowDirection_None);
				break;

			case EGrazVisualizationState_ContinousFeedback:
				drawReferenceCross();
				if (m_bShowFeedback && !m_bDelayFeedback)
				{
					drawBar();
				}
				break;

			case EGrazVisualizationState_Idle:
				if (m_bShowFeedback && m_bDelayFeedback)
				{
					drawBar();
				}
				break;

			default:
				break;
			}
			if (m_bShowAccuracy)
			{
				drawAccuracy();
			}

		}


		// Création de la map
		void CBoxAlgorithmJeuPong::drawField()
		{
			gint l_iWindowWidth = m_pDrawingArea->allocation.width;
			gint l_iWindowHeight = m_pDrawingArea->allocation.height;

			//increase line's width
			gdk_gc_set_line_attributes(m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);


			// Vertical line
			gdk_draw_line(m_pDrawingArea->window,
				m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)],
				(l_iWindowWidth / 2), (l_iWindowHeight / 4),
				(l_iWindowWidth / 2), ((3 * l_iWindowHeight) / 4)
				);
			
			// Drawing of the squares

			gdk_draw_line(m_pDrawingArea->window,
				m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], (l_iWindowWidth * 2 / 20), (l_iWindowHeight * 17 / 25), (l_iWindowWidth * 8 / 20), (l_iWindowHeight * 17 / 25));

			gdk_draw_line(m_pDrawingArea->window,
				m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], (l_iWindowWidth * 12 / 20), (l_iWindowHeight * 17 / 25), (l_iWindowWidth * 18 / 20), (l_iWindowHeight * 17 / 25));

			gdk_draw_line(m_pDrawingArea->window,
				m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], (l_iWindowWidth * 2 / 20), (l_iWindowHeight * 22 / 25), (l_iWindowWidth * 8 / 20), (l_iWindowHeight * 22 / 25));

			gdk_draw_line(m_pDrawingArea->window,
				m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], (l_iWindowWidth * 12 / 20), (l_iWindowWidth * 22 / 25), (l_iWindowWidth * 18 / 20), (l_iWindowWidth * 22 / 25));



			gdk_draw_line(m_pDrawingArea->window,
				m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], (l_iWindowWidth * 2 / 20), (l_iWindowHeight * 17 / 25), (l_iWindowWidth * 2 / 20), (l_iWindowHeight * 22 / 25));

			gdk_draw_line(m_pDrawingArea->window,
				m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], (l_iWindowWidth * 8 / 20), (l_iWindowHeight * 17 / 25), (l_iWindowWidth * 8 / 20), (l_iWindowHeight * 22 / 25));


			gdk_draw_line(m_pDrawingArea->window,
				m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], (l_iWindowWidth * 12 / 20), (l_iWindowHeight * 17 / 25), (l_iWindowWidth * 12 / 20), (l_iWindowHeight * 22 / 25));

			gdk_draw_line(m_pDrawingArea->window,
				m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], (l_iWindowWidth * 18 / 20), (l_iWindowHeight * 17 / 25), (l_iWindowWidth * 18 / 20), (l_iWindowHeight * 22 / 25));


			//increase line's width
			gdk_gc_set_line_attributes(m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);

		}




		void CBoxAlgorithmJeuPong::drawTarget(EArrowDirection eDirection){

			gint l_iWindowWidth = m_pDrawingArea->allocation.width;
			gint l_iWindowHeight = m_pDrawingArea->allocation.height;

			gint l_iX = 0;
			gint l_iY = 0;

			switch (eDirection) 
			{
			case EArrowDirection_Left:
				break;

			case EArrowDirection_Right:


				break;

			default:
				break;

			}
		}



		void CBoxAlgorithmJeuPong::drawBall()
		{
			
			gint l_iWindowWidth = m_pDrawingArea->allocation.width;
			gint l_iWindowHeight = m_pDrawingArea->allocation.height;

			//increase line's width
			gdk_gc_set_line_attributes(m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);


			gint PosBallX = l_iWindowWidth / 2;
			gint PosBallY = l_iWindowHeight * 2 / 25;


			float64 l_f64UsedScale = m_f64BarScale;
			if (m_bPositiveFeedbackOnly)
			{
				// @fixme for multiclass
				const uint32 l_ui32TrueDirection = m_eCurrentDirection - 1;
				const uint32 l_ui32ThisVote = (m_f64BarScale < 0 ? 0 : 1);
				if (l_ui32TrueDirection != l_ui32ThisVote)
				{
					l_f64UsedScale = 0;
				}
			}


			PosBallX = static_cast<gint>(fabs(l_iWindowWidth * fabs(l_f64UsedScale) / 2));

			PosBallX = (PosBallX>(l_iWindowWidth / 2)) ? (l_iWindowWidth / 2) : PosBallX;


			if (m_f64BarScale < 0)
			{

			}

			else 
			{

			}





		}






		void CBoxAlgorithmJeuPong::drawReferenceCross()
		{
			gint l_iWindowWidth = m_pDrawingArea->allocation.width;
			gint l_iWindowHeight = m_pDrawingArea->allocation.height;

			//increase line's width
			gdk_gc_set_line_attributes(m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);

			//horizontal line
			gdk_draw_line(m_pDrawingArea->window,
				m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)],
				(l_iWindowWidth / 4), (l_iWindowHeight / 2),
				((3 * l_iWindowWidth) / 4), (l_iWindowHeight / 2)
				);
			//vertical line
			gdk_draw_line(m_pDrawingArea->window,
				m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)],
				(l_iWindowWidth / 2), (l_iWindowHeight / 4),
				(l_iWindowWidth / 2), ((3 * l_iWindowHeight) / 4)
				);

			//increase line's width
			gdk_gc_set_line_attributes(m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);

		}

		void CBoxAlgorithmJeuPong::drawArrow(EArrowDirection eDirection)
		{
			gint l_iWindowWidth = m_pDrawingArea->allocation.width;
			gint l_iWindowHeight = m_pDrawingArea->allocation.height;

			gint l_iX = 0;
			gint l_iY = 0;

			switch (eDirection)
			{
			case EArrowDirection_None:
				this->drawArrow(EArrowDirection_Left);
				this->drawArrow(EArrowDirection_Right);
				// this->drawArrow(EArrowDirection_Up);
				// this->drawArrow(EArrowDirection_Down);
				break;

			case EArrowDirection_Left:
				l_iX = (l_iWindowWidth / 2) - gdk_pixbuf_get_width(m_pLeftArrow) - 1;
				l_iY = (l_iWindowHeight / 2) - (gdk_pixbuf_get_height(m_pLeftArrow) / 2);
				gdk_draw_pixbuf(m_pDrawingArea->window, NULL, m_pLeftArrow, 0, 0, l_iX, l_iY, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
				break;

			case EArrowDirection_Right:
				l_iX = (l_iWindowWidth / 2) + 2;
				l_iY = (l_iWindowHeight / 2) - (gdk_pixbuf_get_height(m_pRightArrow) / 2);
				gdk_draw_pixbuf(m_pDrawingArea->window, NULL, m_pRightArrow, 0, 0, l_iX, l_iY, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
				break;

			case EArrowDirection_Up:
				l_iX = (l_iWindowWidth / 2) - (gdk_pixbuf_get_width(m_pUpArrow) / 2);
				l_iY = (l_iWindowHeight / 2) - gdk_pixbuf_get_height(m_pUpArrow) - 1;
				gdk_draw_pixbuf(m_pDrawingArea->window, NULL, m_pUpArrow, 0, 0, l_iX, l_iY, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
				break;

			case EArrowDirection_Down:
				l_iX = (l_iWindowWidth / 2) - (gdk_pixbuf_get_width(m_pDownArrow) / 2);
				l_iY = (l_iWindowHeight / 2) + 2;
				gdk_draw_pixbuf(m_pDrawingArea->window, NULL, m_pDownArrow, 0, 0, l_iX, l_iY, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
				break;

			default:
				break;
			}
		}

		void CBoxAlgorithmJeuPong::drawBar()                                               //Dessin de la barre doit être transformer en boule qui bouge
		{
			const gint l_iWindowWidth = m_pDrawingArea->allocation.width;
			const gint l_iWindowHeight = m_pDrawingArea->allocation.height;

			float64 l_f64UsedScale = m_f64BarScale;
			if (m_bPositiveFeedbackOnly) 
			{
				// @fixme for multiclass
				const uint32 l_ui32TrueDirection = m_eCurrentDirection - 1;
				const uint32 l_ui32ThisVote = (m_f64BarScale < 0 ? 0 : 1);
				if (l_ui32TrueDirection != l_ui32ThisVote)
				{
					l_f64UsedScale = 0;
				}
			}

			gint l_iRectangleWidth = static_cast<gint>(fabs(l_iWindowWidth * fabs(l_f64UsedScale) / 2));

			l_iRectangleWidth = (l_iRectangleWidth>(l_iWindowWidth / 2)) ? (l_iWindowWidth / 2) : l_iRectangleWidth;

			const gint l_iRectangleHeight = l_iWindowHeight / 6;

			gint l_iRectangleTopLeftX = l_iWindowWidth / 2;
			const gint l_iRectangleTopLeftY = (l_iWindowHeight / 2) - (l_iRectangleHeight / 2);

			if (m_f64BarScale<0)
			{
				l_iRectangleTopLeftX -= l_iRectangleWidth;

				gdk_pixbuf_render_to_drawable(m_pLeftBar, m_pDrawingArea->window, NULL,
					gdk_pixbuf_get_width(m_pLeftBar) - l_iRectangleWidth, 0,
					l_iRectangleTopLeftX, l_iRectangleTopLeftY, l_iRectangleWidth, l_iRectangleHeight,
					GDK_RGB_DITHER_NONE, 0, 0);
			}
			else
			{
				gdk_pixbuf_render_to_drawable(m_pRightBar, m_pDrawingArea->window, NULL, 0, 0, l_iRectangleTopLeftX, l_iRectangleTopLeftY, l_iRectangleWidth, l_iRectangleHeight, GDK_RGB_DITHER_NONE, 0, 0);

			}
		}

		void CBoxAlgorithmJeuPong::drawAccuracy(void)
		{
			PangoLayout *layout;
			char tmp[512];
			layout = pango_layout_new(gdk_pango_context_get());

			const float64* l_pBuffer = m_oConfusion.getBuffer();

			sprintf(tmp, "L");
			pango_layout_set_text(layout, tmp, -1);
			gdk_draw_layout(m_pDrawingArea->window, m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], 8, 16, layout);

			sprintf(tmp, "%.3d", (int)l_pBuffer[0]);
			pango_layout_set_text(layout, tmp, -1);
			gdk_draw_layout(m_pDrawingArea->window, m_pDrawingArea->style->white_gc, 8 + 16, 16, layout);

			sprintf(tmp, "%.3d", (int)l_pBuffer[1]);
			pango_layout_set_text(layout, tmp, -1);
			gdk_draw_layout(m_pDrawingArea->window, m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], 8 + 56, 16, layout);

			sprintf(tmp, "R");
			pango_layout_set_text(layout, tmp, -1);
			gdk_draw_layout(m_pDrawingArea->window, m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], 8, 32, layout);

			sprintf(tmp, "%.3d", (int)l_pBuffer[2]);
			pango_layout_set_text(layout, tmp, -1);
			gdk_draw_layout(m_pDrawingArea->window, m_pDrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_pDrawingArea)], 8 + 16, 32, layout);

			sprintf(tmp, "%.3d", (int)l_pBuffer[3]);
			pango_layout_set_text(layout, tmp, -1);
			gdk_draw_layout(m_pDrawingArea->window, m_pDrawingArea->style->white_gc, 8 + 56, 32, layout);

			uint32 l_i32Predictions = 0;
			for (uint32 i = 0; i<4; i++) {
				l_i32Predictions += (int)l_pBuffer[i];
			}

			sprintf(tmp, "Acc = %3.1f%%", (l_i32Predictions == 0 ? 0 : 100.0*(l_pBuffer[0] + l_pBuffer[3]) / (float64)l_i32Predictions));
			pango_layout_set_text(layout, tmp, -1);
			gdk_draw_layout(m_pDrawingArea->window, m_pDrawingArea->style->white_gc, 8 + 96, 32, layout);


			g_object_unref(layout);
		}

		float64 CBoxAlgorithmJeuPong::aggregatePredictions(bool bIncludeAll)
		{
			float64 l_f64VoteAggregate = 0;

			// Do we have enough predictions to integrate a result?
			if (m_vAmplitude.size() >= m_i64PredictionsToIntegrate)
			{
				// step backwards with rev iter to take the latest samples
				uint64 count = 0;
				for (std::deque<float64>::reverse_iterator a = m_vAmplitude.rbegin();
					a != m_vAmplitude.rend() && (bIncludeAll || count<m_i64PredictionsToIntegrate); a++, count++)
				{
					l_f64VoteAggregate += *a;
					m_f64MaxAmplitude = std::max<float64>(m_f64MaxAmplitude, abs(*a));
				}

				l_f64VoteAggregate /= m_f64MaxAmplitude;
				l_f64VoteAggregate /= count;

			}

			return l_f64VoteAggregate;
		}

		// @fixme for >2 classes
		void CBoxAlgorithmJeuPong::updateConfusionMatrix(float64 f64VoteAggregate)
		{
			if (m_eCurrentDirection == EArrowDirection_Left || m_eCurrentDirection == EArrowDirection_Right)
			{
				const uint32 l_ui32TrueDirection = m_eCurrentDirection - 1;

				const uint32 l_ui32ThisVote = (f64VoteAggregate < 0 ? 0 : 1);

				(m_oConfusion.getBuffer())[l_ui32TrueDirection * 2 + l_ui32ThisVote]++;

				// std::cout << "Now " << l_ui32TrueDirection  << " vote " << l_ui32ThisVote << "\n";
			}

			// CString out;
			// OpenViBEToolkit::Tools::Matrix::toString(m_oConfusion,out);
			// this->getLogManager() << LogLevel_Info << "Trial conf " << out << "\n";
		}

		void CBoxAlgorithmJeuPong::setMatrixBuffer(const float64* pBuffer)
		{
			if (m_eCurrentState != EGrazVisualizationState_ContinousFeedback)
			{
				// We're not inside a trial, discard the prediction
				return;
			}

			float64 l_f64PredictedAmplitude = 0;
			if (m_bTwoValueInput)
			{
				// Ad-hoc forcing to probability (range [0,1], sum to 1). This will make scaling easier 
				// if run forever in a continuous mode. If the input is already scaled this way, no effect.
				// 
				float64 l_f64Value0 = std::abs(pBuffer[0]);
				float64 l_f64Value1 = std::abs(pBuffer[1]);
				const float64 l_f64Sum = l_f64Value0 + l_f64Value1;
				if (l_f64Sum != 0)
				{
					l_f64Value0 = l_f64Value0 / l_f64Sum;
					l_f64Value1 = l_f64Value1 / l_f64Sum;
				}
				else
				{
					l_f64Value0 = 0.5;
					l_f64Value1 = 0.5;
				}

				//				printf("%f %f\n", l_f64Value0, l_f64Value1);

				l_f64PredictedAmplitude = l_f64Value1 - l_f64Value0;
			}
			else
			{
				l_f64PredictedAmplitude = pBuffer[0];
			}

			m_vAmplitude.push_back(l_f64PredictedAmplitude);

			if (m_bShowFeedback && !m_bDelayFeedback)
			{
				m_f64BarScale = aggregatePredictions(false);

				//printf("bs %f\n", m_f64BarScale);

				gdk_window_invalidate_rect(m_pDrawingArea->window,
					NULL,
					true);
			}
		}


		void CBoxAlgorithmJeuPong::resize(uint32 ui32Width, uint32 ui32Height)
		{
			ui32Width = (ui32Width<8 ? 8 : ui32Width);
			ui32Height = (ui32Height<8 ? 8 : ui32Height);

			if (m_pLeftArrow)
			{
				g_object_unref(G_OBJECT(m_pLeftArrow));
			}

			if (m_pRightArrow)
			{
				g_object_unref(G_OBJECT(m_pRightArrow));
			}

			if (m_pUpArrow)
			{
				g_object_unref(G_OBJECT(m_pUpArrow));
			}

			if (m_pDownArrow)
			{
				g_object_unref(G_OBJECT(m_pDownArrow));
			}

			if (m_pRightBar)
			{
				g_object_unref(G_OBJECT(m_pRightBar));
			}

			if (m_pLeftBar)
			{
				g_object_unref(G_OBJECT(m_pLeftBar));
			}

			m_pLeftArrow = gdk_pixbuf_scale_simple(m_pOriginalLeftArrow, (2 * ui32Width) / 8, ui32Height / 4, GDK_INTERP_BILINEAR);
			m_pRightArrow = gdk_pixbuf_scale_simple(m_pOriginalRightArrow, (2 * ui32Width) / 8, ui32Height / 4, GDK_INTERP_BILINEAR);
			m_pUpArrow = gdk_pixbuf_scale_simple(m_pOriginalUpArrow, ui32Width / 4, (2 * ui32Height) / 8, GDK_INTERP_BILINEAR);
			m_pDownArrow = gdk_pixbuf_scale_simple(m_pOriginalDownArrow, ui32Width / 4, (2 * ui32Height) / 8, GDK_INTERP_BILINEAR);

			m_pRightBar = gdk_pixbuf_scale_simple(m_pOriginalBar, ui32Width, ui32Height / 6, GDK_INTERP_BILINEAR);
			m_pLeftBar = gdk_pixbuf_flip(m_pRightBar, true);
		}

		// Note that we don't need concurrency control here as gtk callbacks run in the main thread
		void CBoxAlgorithmJeuPong::flushQueue(void)
		{
			for (size_t i = 0; i<m_vStimuliQueue.size(); i++)
			{
				m_pStimulusSender->sendStimulation(m_vStimuliQueue[i]);
			}
			m_vStimuliQueue.clear();

			// This function will be automatically removed after completion, so set to 0
			m_uiIdleFuncTag = 0;
		}




	};
};