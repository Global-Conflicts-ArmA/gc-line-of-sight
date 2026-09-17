class GC_LineOfSightUI : SCR_MapUIBaseComponent
{
	// this is basically responsible for:
	// - ui for activating the tool
	// - controlling tool options
	// - setting starting position with cursor
	
	// beyond that, it just makes calls to the system
	
	
	protected SCR_MapToolEntry m_ToolMenuEntry;
	protected SCR_MapCursorModule m_CursorModule;
	protected GC_TracingSystem m_TracingSystem;
	
	protected bool m_bToolActive;
	
	protected float m_fSourceOffset = 1.5;
	protected float m_fTargetOffset = 0.5;
	protected bool m_bColorMode = false;
	
	
	override void Init()
	{
		super.Init();
		
		SCR_MapToolMenuUI toolMenu = SCR_MapToolMenuUI.Cast(m_MapEntity.GetMapUIComponent(SCR_MapToolMenuUI));
		if (!toolMenu)
		{
			m_ToolMenuEntry = toolMenu.RegisterToolMenuEntry("{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset", "terrainIcon", 50);
			m_ToolMenuEntry.m_OnClick.Insert(ToolButtonClicked);
			m_ToolMenuEntry.SetEnabled(true);
		}
		
		m_CursorModule = SCR_MapCursorModule.Cast(m_MapEntity.GetMapModule(SCR_MapCursorModule));
		
		m_TracingSystem = GC_TracingSystem.Cast(GetGame().GetWorld().FindSystem(GC_TracingSystem));
	}
	
	protected void ToolButtonClicked()
	{
		
		// while active, also display settings ui
		
		if (!m_bToolActive)
		{
			AwaitInput();
		}
		else
		{
			StopAwaitInput();
		}
		
		m_bToolActive = !m_bToolActive;
	}
	
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		ActivateTool(); // maybe not
	}
	
	override void OnMapClose(MapConfiguration config)
	{		
		super.OnMapClose(config);
		DeactivateTool();
	}
	
	
	protected void ActivateTool();
	// await click input
	
	protected void DeactivateTool();
	
	
	protected void AwaitInput()
	{
		GetGame().GetInputManager().AddActionListener("MapSelect", EActionTrigger.DOWN, OnMapClick);
	}
	
	protected void StopAwaitInput()
	{
		GetGame().GetInputManager().RemoveActionListener("MapSelect", EActionTrigger.DOWN, OnMapClick);
	}
	
	protected void OnMapClick(float value, EActionTrigger reason)
	{
		StopAwaitInput();
		
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_MAP_GADGET_MARKER_DRAW_START);
		
		float cursorX, cursorY;
		m_MapEntity.GetMapCursorWorldPosition(cursorX, cursorY);
		
		m_TracingSystem.ActivateTool(cursorX, cursorY, m_fSourceOffset, m_fTargetOffset);
	}
	
	
	protected void ToggleColorMode()
	{
		m_bColorMode = !m_bColorMode;
		// visually toggle button in ui
		
		m_TracingSystem.SetColorMode(m_bColorMode);
	}
}
