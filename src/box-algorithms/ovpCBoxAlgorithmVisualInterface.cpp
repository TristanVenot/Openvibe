#include "ovpCBoxAlgorithmVisualInterface.h"

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

namespace OpenViBEPlugins {

	namespace SignalProcessing
	{

		gboolean VisualInterface_SizeAllocateCallback(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
		{
			reinterpret_cast<CBoxAlgorithmVisualInterface*>(data)->resize((uint32)allocation->width, (uint32)allocation->height);
			return FALSE;
		}

		gboolean VisualInterface_RedrawCallback(GtkWidget *widget, GdkEventExpose *event, gpointer data)
		{
			reinterpret_cast<CBoxAlgorithmVisualInterface*>(data)->redraw();
			return TRUE;
		}

		CBoxAlgorithmVisualInterface::CBoxAlgorithmVisualInterface(void) :
			m_pBuilderInterface(NULL),
			m_pMainWindow(NULL),
			m_pDrawingArea(NULL),


			m_visualizationContext(nullptr)
		{
			m_oBackgroundColor.pixel = 0;
			m_oBackgroundColor.red = 0;
			m_oBackgroundColor.green = 0;
			m_oBackgroundColor.blue = 0;

			m_oForegroundColor.pixel = 0;
			m_oForegroundColor.red = 0xFFFF;
			m_oForegroundColor.green = 0xFFFF;
			m_oForegroundColor.blue = 0xFFFF;
		}

		bool CBoxAlgorithmVisualInterface::initialize(void)
		{
			m_oInput0Decoder.initialize(*this, 0);
			m_oInput1Decoder.initialize(*this, 1);



			return true;
		}
		/*******************************************************************************/

		bool CBoxAlgorithmVisualInterface::uninitialize(void)
		{
			m_oInput0Decoder.uninitialize();
			m_oInput1Decoder.uninitialize();


			return true;
		}
		/*******************************************************************************/




		bool CBoxAlgorithmVisualInterface::process(void)
		{

			// the static box context describes the box inputs, outputs, settings structures
			const IBox& l_rStaticBoxContext = this->getStaticBoxContext();
			// the dynamic box context describes the current state of the box inputs and outputs (i.e. the chunks)
			IBoxIO& l_rDynamicBoxContext = this->getDynamicBoxContext();




			// Tutorials:
			// http://openvibe.inria.fr/documentation/#Developer+Documentation
			// Codec Toolkit page :
			// http://openvibe.inria.fr/codec-toolkit-references/

			// Feel free to ask experienced developers on the forum (http://openvibe.inria.fr/forum) and IRC (#openvibe on irc.freenode.net).

			return true;
		}
		void CBoxAlgorithmVisualInterface::resize(uint32 ui32Width, uint32 ui32Height){}

		void CBoxAlgorithmVisualInterface::redraw(){}

		void CBoxAlgorithmVisualInterface::activate()
		{
			char buffer[100];
			int retVal;
			char name[] = "Max";
			int age = 23;

			retVal = sprintf(buffer, "Hi, I am %s and I am %d years old", name, age);

			gint speedX = m_pDrawingArea->allocation.width;

		}
	};
};
