
#include "stdafx.h"
#include "DimensionDataLayer.h"


ref<CDimensions> db::dimensions::Create(double length, double width, double height)
{
	return CDimensions::create(length, width, height);
}

ref<CDimensions> db::dimensions::Clone(ref<CDimensions> const& source)
{
	r_ref<CDimensions> r_source = source;
	return CDimensions::create(
			r_source->GetLength(),
			r_source->GetWidth(),
			r_source->GetHeight());
}
