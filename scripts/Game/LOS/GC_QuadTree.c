class GC_QuadNode
{	
	int m_iEntsBlocked;
	int m_iTerrBlocked;

	//! Node bounds
	float m_fX1;
	float m_fX2;
	float m_fY1;
	float m_fY2;

	//! Node level
	int m_iLevel;

	//! Index in list of active nodes
	int m_iActiveIndex = -1;
	
	//! Index in list of draw commands
	int m_iCommandIndex = -1;
	
	ref GC_QuadNode m_Q1;
	ref GC_QuadNode m_Q2;
	ref GC_QuadNode m_Q3;
	ref GC_QuadNode m_Q4;
	
	GC_QuadNode m_Parent; // no strong ref!
	
	ref PolygonDrawCommand m_DrawCommand;
	
	bool m_bTransparentColor;


	void GC_QuadNode(GC_QuadNode parent, vector fromPos, float toOffset, int level, float x1, float x2, float y1, float y2)
	{
		m_Parent = parent;
		m_iLevel = level;
		m_fX1 = x1;
		m_fX2 = x2;
		m_fY1 = y1;
		m_fY2 = y2;
		
		const BaseWorld world = GetGame().GetWorld();
		
		const float quarterX = (m_fX2 - m_fX1) / 4;
		const float quarterY = (m_fY2 - m_fY1) / 4;
		
		SingleTrace(world, fromPos, toOffset, m_fX2 - quarterX, m_fY2 - quarterY);
		SingleTrace(world, fromPos, toOffset, m_fX1 + quarterX, m_fY2 - quarterY);
		SingleTrace(world, fromPos, toOffset, m_fX1 + quarterX, m_fY1 + quarterY);
		SingleTrace(world, fromPos, toOffset, m_fX2 - quarterX, m_fY1 + quarterY);
	}
	
	protected void SingleTrace(BaseWorld world, vector fromPos, float toOffset, float to1, float to2)
	{
		const vector toPos = Vector(to1, Math.Max(0, world.GetSurfaceY(to1, to2)) + toOffset, to2);
		if (SightBlocked(world, fromPos, toPos, TraceFlags.WORLD, EPhysicsLayerDefs.Terrain))
			m_iTerrBlocked++;
		else if (SightBlocked(world, fromPos, toPos, TraceFlags.ENTS, EPhysicsLayerDefs.ViewGeometry))
			m_iEntsBlocked++;
	}
	
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
	
	void CreateCommand()
	{
		m_DrawCommand = new PolygonDrawCommand();
		m_DrawCommand.m_Vertices = {0, 0, 0, 0, 0, 0, 0, 0};
	}
	
	void UpdateColor(GC_ShadingMode mode)
	{
		const int anyBlocked = m_iTerrBlocked + m_iEntsBlocked;
		
		if (mode == GC_ShadingMode.Darken)
			m_DrawCommand.m_iColor = ARGBF(anyBlocked * 0.2, 0, 0, 0);
		else if (mode == GC_ShadingMode.Blacken)
			m_DrawCommand.m_iColor = ARGBF(anyBlocked * 0.25, 0, 0, 0); // 0 alpha if none blocked, 1 alpha if all blocked
		else if (mode == GC_ShadingMode.Obstacle && anyBlocked != 0)
			m_DrawCommand.m_iColor = ARGBF(anyBlocked * 0.15, 1, m_iEntsBlocked * 0.75 / anyBlocked, 0); // red 1, green 0 to 0.75, blue 0 => red to yellow gradient
		else if (mode == GC_ShadingMode.DebugVis)
			m_DrawCommand.m_iColor = ARGBF(0.5, Math.RandomFloat01(), Math.RandomFloat01(), Math.RandomFloat01());
		else
			m_DrawCommand.m_iColor = ARGBF(0, 0, 0, 0);
		
		m_bTransparentColor = (m_DrawCommand.m_iColor >> 24) & 0xFF == 0; // alpha is 0
	}
}