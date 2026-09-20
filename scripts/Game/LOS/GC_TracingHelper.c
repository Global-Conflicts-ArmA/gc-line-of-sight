class GC_TracingHelper
{
	//! Receives 2 bounding boxes and checks for intersection (2 must be greater than 1)
	static bool BboxIntersects(float ax1, float ax2, float ay1, float ay2, float bx1, float bx2, float by1, float by2)
	{
		return ax2 > bx1 && ax1 < bx2 && ay2 > by1 && ay1 < by2;
	}


		// 2d cases: a b, each 1<2

		// -----    _______ (excludes)		a2 < b1
		// -----______		(touches)		a2 <= b1
		// ----===_____ 	(overlaps)		a1 < a2
		// ----====----- 	(includes)		a1 < b1 < b2 < A2

		// INTERSECTION = no overlap
		// OVERLAP = excludes/touches on one axis


	static bool SightBlocked(BaseWorld world, vector fromPos, vector toPos, TraceFlags flags, EPhysicsLayerDefs layer, float tolerance = 1.0)
	{
		TraceParam trace = new TraceParam();
		trace.Start = fromPos;
		trace.End = toPos;
		trace.Flags = flags;
		trace.TargetLayers = layer;
		
		float frac = world.TraceMove(trace);
		float dist = vector.Distance(trace.Start, trace.End);
		
		return (frac < (dist - tolerance) / dist);
	}
}