class GC_QuadNode
{
	protected int m_color;

	//! Node bounds
	int m_iX1;
	int m_iX2;
	int m_iY1;
	int m_iY2;

	//! Node level
	int m_iLevel;

	//! Index in list of active nodes
	int m_iActiveIndex = -1;
	
	ref GC_QuadNode m_Q1;
	ref GC_QuadNode m_Q2;
	ref GC_QuadNode m_Q3;
	ref GC_QuadNode m_Q4;
	
	ref PolygonDrawCommand m_DrawCommand;


	void GC_QuadNode(vector fromPos, float toOffset, bool colorMode)
	{
		vector toPos; // center of quad node, with y = Get terrain y + toOffset
		
		BaseWorld world = GetGame().GetWorld();
		
		if (!colorMode)
		{
			// perform 4 visibility traces
			int visCount = GC_TracingHelper.SightTrace(world, fromPos, toPos,TraceFlags.ENTS, EPhysicsLayerDefs.ViewGeometry); // does this capture terrain?
		}
		else
		{
			// perform 4 vis and 4 terrain traces, then blend between red and orange multiplied by brightness
		}
		
		/*
		if (!GC_TracingHelper.SightTrace(world, fromPos, toPos,TraceFlags.WORLD, EPhysicsLayerDefs.Terrain))
			//m_Trace = GC_SightTraceResult.Terrain;
		else if (!HasVisibility(world, fromPos, toPos, TraceFlags.ENTS, EPhysicsLayerDefs.ViewGeometry))
			///m_Trace = GC_SightTraceResult.ViewGeo;
		else
			//m_Trace = GC_SightTraceResult.Free;
		*/


		// also set node bounds and level here
	}
	
	protected GC_SightTraceResult PerformTrace(BaseWorld world, vector fromPos, vector toOffset)
	{
		vector toPos; // center of quad node, with y = Get terrain y + toOffset
		if (!GC_TracingHelper.SightTrace(world, fromPos, toPos,TraceFlags.WORLD, EPhysicsLayerDefs.Terrain))
			return GC_SightTraceResult.Terrain;
		else if (!GC_TracingHelper.SightTrace(world, fromPos, toPos, TraceFlags.ENTS, EPhysicsLayerDefs.ViewGeometry))
			return GC_SightTraceResult.ViewGeo;
		else
			return GC_SightTraceResult.Free;
	}

	void UpdateCommand() // receive map view. also needs own intended bounds, either passed or local knowledge
	{						// local knowledge MAY be preferable because updatecommand cannot pass it
		if (!m_DrawCommand)
		{
			m_DrawCommand = new PolygonDrawCommand();
			m_DrawCommand.m_iColor = 0; // set color based on m_Trace
		}
	}
}

class GC_NodeQueue
{
	protected ref GC_QueueElement m_Start;
	protected GC_QueueElement m_End;

	void Enqueue(GC_QuadNode node)
	{
		GC_QueueElement element = new GC_QueueElement(node);
		if (m_Start)
		{
			m_End.m_Next = element;
			m_End = element;
		}
		else
		{
			m_Start = element;
			m_End = element;
		}
	}

	GC_QuadNode Deque()
	{
		GC_QuadNode node;

		if (m_Start)
		{
			node = m_Start.m_Data;
			m_Start = m_Start.m_Next;
		}

		return node;
	}

	bool IsEmpty()
	{
		return m_Start;
	}

	void Clear()
	{
		m_Start = null;
		m_End = null;
	}
}

class GC_QueueElement
{
	ref GC_QueueElement m_Next;
	GC_QuadNode m_Data;

	void GC_QueueElement(GC_QuadNode node)
	{
		m_Data = node;
	}
}