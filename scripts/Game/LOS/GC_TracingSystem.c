class GC_TracingSystem : GameSystem
{
	protected SCR_MapEntity m_MapEntity;
	
	//! Root node of quad tree
	protected ref GC_QuadNode m_QuadTree;
	
	//! Nodes that should have draw commands
	protected ref array<GC_QuadNode> m_ActiveNodes;
	
	//! Array of draw commands, rebuilt frequently
	protected ref array<ref PolygonDrawCommand> m_aDrawCommands = null;

	//! Node queue
	protected ref GC_SimpleQueue<GC_QuadNode> m_NodeQueue = new GC_SimpleQueue<GC_QuadNode>();

	
	protected int m_iMaintenanceBudget = 1000;
	protected int m_iSubdivCost = 100;
	
	protected vector m_vSourcePos;
	protected float m_fTargetOffset;
	protected int m_iLevel;
	
	
	
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

		// must always freshly redo tree when map changed, but can save progress and continue if map did not change
		
		if (mapChange) // change, restart maintenance
		{
			m_NodeQueue.Clear();
			MaintainTree();
			UpdateCommands();
		}
		else if (!m_NodeQueue.IsEmpty()  && MaintainTree()) // no change, but not done maintaining
		{
			UpdateCommands();
		}
		
		m_vPreviousPan = currentPan;
		m_fPreviousZoom = currentZoom;

	}
	
	//! Maintains the quad tree, removing obsolete nodes and adding required nodes. Returns whether a command update it needed.
	protected bool MaintainTree()
	{
		bool needUpdate = false;
		
		 // could also save this on the class and process it across frames
		int intendedLevel = 5;
		int frameCost = 0;
		bool mode = false;

		if (!m_QuadTree) // only do this if the queue is empty, or maybe do this elsewhere entirely
		{
			m_QuadTree = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, mode);
			// how do i ensure the root node exists? i guess i could just always make sure it's initialized here after running global box intersection
			// and other requirements from the loop
			// could also create it elsewhere and make sure it's never deleted
		}
		m_NodeQueue.Enqueue(m_QuadTree);

		GC_QuadNode node = m_NodeQueue.Deque();
		while (node && frameCost < m_iMaintenanceBudget)
		{
			frameCost += 1;
			bool levelReached = node.m_iLevel >= intendedLevel;
			bool intersection; //GC_TracingHelper.BboxIntersects(viewMin, viewMax, node.m_Min, node.m_Max);
			bool hasChildren = 0 > node.m_iActiveIndex;
			
			
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
					node.m_Q1 = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, mode);
					node.m_Q2 = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, mode);
					node.m_Q3 = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, mode);
					node.m_Q4 = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, mode);
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
		
		return needUpdate;

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
			m_ActiveNodes.Remove(index);
			if (index < m_ActiveNodes.Count())
				m_ActiveNodes[index].m_iActiveIndex = index;
		}
	}

	void ActivateNode(GC_QuadNode node)
	{
		if (node.m_iActiveIndex < 0)
		{
			m_ActiveNodes.Insert(node);
			node.m_iActiveIndex = m_ActiveNodes.Count() - 1;
		}
	}

	protected void UpdateCommands()
	{
		foreach (GC_QuadNode node : m_ActiveNodes)
		{
			node.UpdateCommand(); // pass current map view
		}

		// also update source marker maybe (or possibly do this elsewhere)
	}
	
	

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