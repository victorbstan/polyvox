%module SimpleVolume
%{
#include "Material.h"
#include "Density.h"
#include "SimpleVolume.h"
%}

%import "BaseVolume.h"
%include "Material.h"
%include "Density.h"
%include "SimpleVolume.h"

%template(BaseVolumeDensity8) PolyVox::BaseVolume<PolyVox::Density8>;

//%template(SimpleVolumeMaterial8) PolyVox::SimpleVolume<PolyVox::Material8>;
%template(SimpleVolumeDensity8) PolyVox::SimpleVolume<PolyVox::Density8>;