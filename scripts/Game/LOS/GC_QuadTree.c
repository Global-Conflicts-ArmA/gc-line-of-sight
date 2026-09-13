class GC_QuadNode
{
	protected int m_iColor;
	
	protected int m_iEntsTraces;
	protected int m_iTerrTraces;

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
		
		// 4 of these, with toPos Y set to terr offset
		m_iTerrTraces += GC_TracingHelper.SightTrace(world, fromPos, toPos, TraceFlags.WORLD, EPhysicsLayerDefs.Terrain); // can also trace WORLD & ENTS but WORLD is very cheap anyway
		m_iEntsTraces += GC_TracingHelper.SightTrace(world, fromPos, toPos, TraceFlags.ENTS, EPhysicsLayerDefs.ViewGeometry);
		
		
		
		
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

class GC_SimpleQueue<Class T>
{
	protected ref GC_QueueElement<T> m_Start;
	protected GC_QueueElement<T> m_End;

	void Enqueue(T item)
	{
		GC_QueueElement<T> element = new GC_QueueElement<T>(item);
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

	T Deque()
	{
		T item;

		if (m_Start)
		{
			item = m_Start.m_Item;
			m_Start = m_Start.m_Next;
		}

		return item;
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

class GC_QueueElement<Class T>
{
	ref GC_QueueElement<T> m_Next;
	T m_Item;

	void GC_QueueElement(T item)
	{
		m_Item = item;
	}
}