#include "Constants.h"
#include "SurfaceVertex.h"

namespace Ogre
{
	SurfaceVertex::SurfaceVertex()
	{
	}

	SurfaceVertex::SurfaceVertex(UIntVector3 positionToSet)
		:position(positionToSet)
	{
	}

	SurfaceVertex::SurfaceVertex(UIntVector3 positionToSet, Vector3 normalToSet)
		:position(positionToSet)
		,normal(normalToSet)
	{
	}

	bool SurfaceVertex::operator==(const SurfaceVertex& rhs) const
	{
		//We dont't check the normal here as it may not have been set. But if two vertices have the same position they should have the same normal too.
		/*return
		(
			(position.x == rhs.position.x) && 
			(position.y == rhs.position.y) && 
			(position.z == rhs.position.z) //&& 
			//(abs(alpha - rhs.alpha) <= 0.01)
		);*/

		/*ulong value = 0;
		value |= position.x;
		value << 10;
		value |= position.y;
		value << 10;
		value |= position.z;

		ulong rhsValue = 0;
		rhsValue |= rhs.position.x;
		rhsValue << 10;
		rhsValue |= rhs.position.y;
		rhsValue << 10;
		rhsValue |= rhs.position.z;

		return value == rhsValue;*/

		unsigned long offset = (position.x*(OGRE_REGION_SIDE_LENGTH*2+1)*(OGRE_REGION_SIDE_LENGTH*2+1)) + (position.y*(OGRE_REGION_SIDE_LENGTH*2+1)) + (position.z);
		unsigned long rhsOffset = (rhs.position.x*(OGRE_REGION_SIDE_LENGTH*2+1)*(OGRE_REGION_SIDE_LENGTH*2+1)) + (rhs.position.y*(OGRE_REGION_SIDE_LENGTH*2+1)) + (rhs.position.z);

		return offset == rhsOffset;
	}

	bool SurfaceVertex::operator < (const SurfaceVertex& rhs) const
	{
		/*if((*this) == rhs)
		{
			return false;
		}
		if(alpha < rhs.alpha)
		{
			return true;
		}*/
		/*if(position.z < rhs.position.z)
		{
			return true;
		}
		if(position.y < rhs.position.y)
		{
			return true;
		}
		if(position.x < rhs.position.x)
		{
			return true;
		}
		
		return false;*/

		/*ulong value = 0;
		value |= position.x;
		value << 10;
		value |= position.y;
		value << 10;
		value |= position.z;

		ulong rhsValue = 0;
		rhsValue |= rhs.position.x;
		rhsValue << 10;
		rhsValue |= rhs.position.y;
		rhsValue << 10;
		rhsValue |= rhs.position.z;

		return value < rhsValue;*/

		unsigned long offset = (position.x*(OGRE_REGION_SIDE_LENGTH*2+1)*(OGRE_REGION_SIDE_LENGTH*2+1)) + (position.y*(OGRE_REGION_SIDE_LENGTH*2+1)) + (position.z);
		unsigned long rhsOffset = (rhs.position.x*(OGRE_REGION_SIDE_LENGTH*2+1)*(OGRE_REGION_SIDE_LENGTH*2+1)) + (rhs.position.y*(OGRE_REGION_SIDE_LENGTH*2+1)) + (rhs.position.z);

		return offset < rhsOffset;
	}
}
