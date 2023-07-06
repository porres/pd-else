/*
 *  mdaSpecMeterController.cpp
 *  mda-vst3
 *
 *  Created by Arne Scheffler on 6/14/08.
 *
 *  mda VST Plug-ins
 *
 *  Copyright (c) 2008 Paul Kellett
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "mdaSpecMeterController.h"

namespace Steinberg {
namespace Vst {
namespace mda {

//-----------------------------------------------------------------------------
SpecMeterController::SpecMeterController ()
{
}

//-----------------------------------------------------------------------------
SpecMeterController::~SpecMeterController ()
{
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API SpecMeterController::initialize (FUnknown* context)
{
	tresult res = BaseController::initialize (context);
	if (res == kResultTrue)
	{
		ParamID pid = kBandParamStart;
		parameters.addParameter (new BaseParameter (USTRING("Left Band 1"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left Band 2"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left Band 3"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left Band 4"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left Band 5"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left Band 6"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left Band 7"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left Band 8"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left Band 9"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left Band 10"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left Band 11"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left Band 12"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left Band 13"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));

		parameters.addParameter (new BaseParameter (USTRING("Right Band 1"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right Band 2"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right Band 3"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right Band 4"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right Band 5"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right Band 6"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right Band 7"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right Band 8"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right Band 9"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right Band 10"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right Band 11"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right Band 12"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right Band 13"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));

		pid = kLeftPeakParam;
		parameters.addParameter (new ScaledParameter (USTRING("Left Peak"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++, 0, 2));
		parameters.addParameter (new ScaledParameter (USTRING("Left Hold"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++, 0, 2));
		parameters.addParameter (new BaseParameter (USTRING("Left Min"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Left RMS"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));

		parameters.addParameter (new ScaledParameter (USTRING("Right Peak"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++, 0, 2));
		parameters.addParameter (new ScaledParameter (USTRING("Right Hold"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++, 0, 2));
		parameters.addParameter (new BaseParameter (USTRING("Right Min"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));
		parameters.addParameter (new BaseParameter (USTRING("Right RMS"), nullptr, 0, 0, ParameterInfo::kIsReadOnly, pid++));

		parameters.addParameter (new BaseParameter (USTRING("Correlation"), 0, 0, 0, ParameterInfo::kIsReadOnly, pid++));
	}
	return res;
}

}}} // namespaces
