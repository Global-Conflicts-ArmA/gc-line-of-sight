class GC_TracingSystem : GameSystem
{
	protected SCR_MapEntity m_MapEntity;
	
	//! Root node of quad tree
	protected ref GC_QuadNode m_QuadTree;
	
	//! Nodes that should have draw commands
	protected ref array<GC_QuadNode> m_ActiveNodes;
	
	//! Array of draw commands, rebuilt frequently
	protected ref array<ref PolygonDrawCommand> m_aDrawCommands = null;
	
	protected int m_iTraceBudget = 10;
	
	
	
	override static void InitInfo(WorldSystemInfo outInfo)
	{
		super.InitInfo(outInfo);
		outInfo
			.SetAbstract(false)
			.SetUnique(true)
			.SetLocation(WorldSystemLocation.Client)
	}
	
	override void OnInit()
	{
		super.OnInit();
		
		Enable(false);
		
		m_MapEntity = SCR_MapEntity.GetMapInstance();
	}
	
	protected vector m_vPreviousPan;
	protected float m_fPreviousZoom;
	
	override void OnUpdate(WorldSystemPoint point)
	{
		super.OnUpdate(point);
		
		vector currentPan = m_MapEntity.GetCurrentPan();
		float currentZoom = m_MapEntity.GetCurrentZoom();
		
		bool mapChange = (m_vPreviousPan != currentPan) || (m_fPreviousZoom != currentZoom);
		
		if (!mapChange)
			return;
		
		m_vPreviousPan = currentPan;
		m_fPreviousZoom = currentZoom;
		
	}
	
	protected void MaintainTree()
	{
		array<GC_QuadNode> nodeQueue = { m_QuadTree };
		int queueIndex = 0;
		
		int traceCount = 0;
		// make sure exists and traced
		int intendedLevel = 5;
		for (int i = 0; i < intendedLevel && nodeQueue.Count() > queueIndex; i++)
		{
			// check if current node is in view and further subdivision is required (level)
			// if not, null children (if not null already)
			// if yes, create q1,q2,q3,q4 and trace them (unless they were already, or trace limit is reached)
			// 	then enqueue these nodes
			
		}
	}
	
	protected void DecideActive()
	{
	}
	
	protected void UpdateCommands()
	{
	}
	
	// task 1: maintain an adequate tree for the current map view
	// - receives current map view BB and zoom level
	//	 calculate intended depth level => do breadth first traversal of tree (frame budget) up to the level
	//			if node child is within map view and not over level, make sure it exists
	//			else make sure it does not exist
	// - determines what parts of the quadtree should be loaded at which resolution
	// - queues relevant traces
	// - somehow, initiates deletion of irrelevant nodes. maybe just a post-move traversal that goes down only to intended depth and deletes any child references
	// task 2: selecting "active" nodes to be drawn, of what data is available, these just get stored in an array
	// - there is a computed ideal for the current map view
	// task 3: every frame, if the map moved, update vertices of currently shown polygons
	// - there is an array of polygondrawcommands
	// - there is also an array of currently active quad nodes which have draw commands
	// - if there was a map change, go through active nodes array and update their draw commands
	// - maybe avoid world coords entirely and only go with map coords
	
	// - quadnode has methods for creating and updating draw command based on current map view. maybe even stored directly on the node
}