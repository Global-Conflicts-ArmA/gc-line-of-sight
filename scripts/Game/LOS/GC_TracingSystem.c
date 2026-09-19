class GC_TracingSystem : GameSystem
{
	protected SCR_MapEntity m_MapEntity;
	
	protected CanvasWidget m_wCanvasWidget;
	
	//! Root node of quad tree
	protected ref GC_QuadNode m_QuadTree;
	
	//! Nodes that should have draw commands
	protected ref array<GC_QuadNode> m_aActiveNodes;
	
	//! Array of draw commands, rebuilt frequently
	protected ref array<ref CanvasWidgetCommand> m_aDrawCommands = null;

	//! Node queue
	protected ref GC_SimpleQueue<GC_QuadNode> m_NodeQueue = new GC_SimpleQueue<GC_QuadNode>();
	
	//! Shading mode
	protected GC_ShadingMode m_bShadingMode = GC_ShadingMode.Darken;

	
	protected int m_iMaintenanceBudget = 1000;
	protected int m_iSubdivCost = 100;
	
	protected vector m_vSourcePos;
	protected float m_fTargetOffset;
	
	
	override static void InitInfo(WorldSystemInfo outInfo)
	{
		super.InitInfo(outInfo);
		outInfo
			.SetAbstract(false)
			.SetUnique(true)
			.SetLocation(WorldSystemLocation.Client)
			.AddPoint(ESystemPoint.PostFrame);
	}
	
	//! Global init, even if the tool is not active yet
	override void OnInit()
	{
		super.OnInit();
		
		Enable(false);
		
		m_MapEntity = SCR_MapEntity.GetMapInstance();
	}
	
	void ActivateTool(float worldX, float worldY, float sourceOffset, float targetOffset)
	{
		Print("Activating tracing system");
		
		m_vSourcePos = Vector(worldX, Math.Max(0, GetGame().GetWorld().GetSurfaceY(worldX, worldY)) + sourceOffset, worldY);
		m_fTargetOffset = targetOffset;
		
		Init();
		
		
		Enable(true);
	}
	
	//! Tool reset (tool or map is closed etc)
	void DeactivateTool()
	{
		Print("Deactivating tracing system");
		
		m_NodeQueue.Clear();
		m_QuadTree = null;
		m_aActiveNodes.Clear();
		m_wCanvasWidget = null;
		m_aDrawCommands = null;
		
		Enable(false);
	}
	
	//! Tool init (tool is opened etc)
	protected void Init()
	{
		vector offset = m_MapEntity.Offset();
		vector size = m_MapEntity.Size();
		m_QuadTree = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, 0, offset[0], offset[0] + size[0], offset[2], offset[2] + size[2]);
		
		Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			mapFrame = m_MapEntity.GetMapMenuRoot();
		if (!mapFrame)
			 return;
		
		m_wCanvasWidget = CanvasWidget.Cast(GetGame().GetWorkspace().CreateWidgets("{F928661E727CC638}UI/Map/GC_LOSCanvas.layout", mapFrame));
		m_aDrawCommands = {};
		m_wCanvasWidget.SetDrawCommands(m_aDrawCommands);
		m_aActiveNodes = {};
	}
	
	protected vector m_vPreviousPan;
	protected float m_fPreviousZoom;
	protected bool m_bMapChanged;
	
	//! Frame update event
	override void OnUpdate(WorldSystemPoint point)
	{
		super.OnUpdate(point);
		
		vector currentPan = m_MapEntity.GetCurrentPan();
		float currentZoom = m_MapEntity.GetCurrentZoom();
		
		bool mapChange = (m_vPreviousPan != currentPan) || (m_fPreviousZoom != currentZoom);
		
		if (mapChange)
			foreach (GC_QuadNode node : m_aActiveNodes)
				node.UpdateVertices(m_MapEntity);
		else if (m_bMapChanged)
			MaintainTree(true);
		else if (!m_NodeQueue.IsEmpty())
			MaintainTree(false);
		
		m_vPreviousPan = currentPan;
		m_fPreviousZoom = currentZoom;
		m_bMapChanged = mapChange;

	}
		
		// tree states:
		// - no map change, queue empty => do nothing
		// - no map change, queue not empty => keep working on it (and update new or all commands)
		// - map change => clear node queue (?) then do maintenance, finally update all commands
		
		// should we clear the queue on map change? possibilities:
		// - clear it on map change then immediately restart maintenance
		// - clear it but wait until post change to restart maintenance
		// - clear it only after map change
		// this is also relevant when the queue is empty, when do we restart maintenance in general?
		// - only after map move
		// - on map move
	
	
	void SetShadingMode(GC_ShadingMode mode)
	{
		if (m_bShadingMode != mode)
		{
			m_bShadingMode = mode;
			foreach (GC_QuadNode node : m_aActiveNodes)
				node.UpdateColor(m_bShadingMode);
		}
	}
	
	//! Maintains the quad tree, removing obsolete nodes and adding required nodes.
	protected void MaintainTree(bool restart)
	{
		
		int intendedLevel = 5;
		
		if (restart)
		{
			m_NodeQueue.Clear();
			m_NodeQueue.Enqueue(m_QuadTree);
			ActivateNode(m_QuadTree);
		}
		
		int frameCost = 0;
		
		vector frameMin, frameMax;
		m_MapEntity.GetMapVisibleFrame(frameMin, frameMax);
		const float frameX1 = frameMin[0];
		const float frameX2 = frameMax[0];
		const float frameY1 = frameMin[2];
		const float frameY2 = frameMax[2];

		GC_QuadNode node = m_NodeQueue.Deque();
		while (node && frameCost < m_iMaintenanceBudget)
		{
			frameCost += 1;
			const bool levelReached = node.m_iLevel >= intendedLevel;
			const bool intersection = GC_TracingHelper.BboxIntersects(frameX1, frameX2, frameY1, frameY2, node.m_fX1, node.m_fX2, node.m_fY1, node.m_fY2);
			const bool hasChildren = 0 > node.m_iActiveIndex;
			
			
			if (levelReached || !intersection)
			{
				// null + deactivate children, make self active
				if (hasChildren)
				{
					DeactivateChildren(node);
					node.m_Q1 = null;
					node.m_Q2 = null;
					node.m_Q3 = null;
					node.m_Q4 = null;
					ActivateNode(node);
				}
			}
			else
			{
				if (!hasChildren)
				{
					// create and activate children, deactivate self
					frameCost += m_iSubdivCost;
					
					float midX = node.m_fX1 + (node.m_fX2 - node.m_fX1) / 2;
					float midY = node.m_fY1 + (node.m_fY2 - node.m_fY1) / 2;
					
					//  II   I
					// III  IV
					
					node.m_Q1 = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, node.m_iLevel + 1, midX, node.m_fX2, midY, node.m_fY2);
					node.m_Q2 = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, node.m_iLevel + 1, node.m_fX1, midX, midY, node.m_fY2);
					node.m_Q3 = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, node.m_iLevel + 1, node.m_fX1, midX, node.m_fY1, midY);
					node.m_Q4 = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, node.m_iLevel + 1, midX, node.m_fX2, node.m_fY1, midY);
					
					DeactivateNode(node);
					ActivateNode(node.m_Q1);
					ActivateNode(node.m_Q2);
					ActivateNode(node.m_Q3);
					ActivateNode(node.m_Q4);
				}
				// enqueue children
				m_NodeQueue.Enqueue(node.m_Q1);
				m_NodeQueue.Enqueue(node.m_Q2);
				m_NodeQueue.Enqueue(node.m_Q3);
				m_NodeQueue.Enqueue(node.m_Q4);
			}
			
			node = m_NodeQueue.Deque();
			
		}

		// check if current node has view intersection and further subdivision is required (level)
		// 	if not, null children and make self active (if not null already). still need to deactivate them though
		// 	if yes, create q1,q2,q3,q4 and trace them (unless they were already, or trace limit is reached) then activate them and deactivate self
		// 		then enqueue these nodes
			
	}
	
	void DeactivateChildren(GC_QuadNode node)
	{
		SCR_Stack<GC_QuadNode> nodeStack = new SCR_Stack<GC_QuadNode>;
		nodeStack.Push(node.m_Q1);
		nodeStack.Push(node.m_Q2);
		nodeStack.Push(node.m_Q3);
		nodeStack.Push(node.m_Q4);
		
		while (!nodeStack.IsEmpty())
		{
			node = nodeStack.Pop();
			if (node.m_Q1)
			{
				nodeStack.Push(node.m_Q1);
				nodeStack.Push(node.m_Q2);
				nodeStack.Push(node.m_Q3);
				nodeStack.Push(node.m_Q4);
			}
			if (node.m_iActiveIndex >= 0)
				DeactivateNode(node);
		}
	}

	void DeactivateNode(GC_QuadNode node)
	{
		int index = node.m_iActiveIndex;
		if (index >= 0)
		{
			node.m_iActiveIndex = -1;
			m_aDrawCommands.Remove(index);
			m_aActiveNodes.Remove(index);
			if (index < m_aActiveNodes.Count())
				m_aActiveNodes[index].m_iActiveIndex = index;
		}
	}

	void ActivateNode(GC_QuadNode node)
	{
		if (node.m_iActiveIndex < 0) // this if might be redundant
		{
			m_aActiveNodes.Insert(node);
			node.m_iActiveIndex = m_aActiveNodes.Count() - 1;
			
			node.CreateCommand(m_bShadingMode);
			node.UpdateVertices(m_MapEntity);
			m_aDrawCommands.Insert(node.m_DrawCommand);
		}
	}
	
	// on node activation, add draw command to array. on deactivation, remove
	
	

	// how to i make sure to not regenerate all draw commands every time

	// in case of level change, need to traverse the entire tree, in case of map move, only the delta areas
	// when traversing the tree, and arrived at the final level, could add the nodes to drawcommands directly (or remove directly when creating children)
	// removal can be done via a draw commands array index saved on the node itself (need to change swapback index too)

	// i should probably make a difference between loadLevel and displayLevel maybe

	// as a prerequisite of stage 1, check if the change was even significant enough maybe
	
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


	// question: do i want to traverse the entire quadtree every frame? could have a shared budget between nodes and traces then stop
	// main question, is the amount of nodes to traverse so great that it warrants the extra cost of keeping track of node visits


}