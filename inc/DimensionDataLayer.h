
#pragma once

#include "Dimensions.h"

namespace db
{
	namespace dimensions
	{
		ref<CDimensions> Create(double length, double width, double height);
		ref<CDimensions> Clone(ref<CDimensions> const& source);

	}	// namespace dimensions
}	// namespace db 